#pragma once

#include <string>

#include <crow_all.h>

#include "ticketeer/core/context.hpp"
#include "ticketeer/db/base.hpp"

namespace ticketeer::apps::helpdesk::roles::supervisor::handlers {

namespace internal {

[[nodiscard]] std::optional<const char *>
GetTicketIdValue(const crow::json::rvalue &item, std::string &s);

[[nodiscard]] std::optional<const char *>
GetTechnicianIdValue(const crow::json::rvalue &item, std::string &s);

} // namespace internal

[[nodiscard]] static inline auto GetStatusIds(auto &db) {
  auto res = db.Exec("SELECT default_status_id, assigned_status_id FROM "
                     "ticketeer.helpdesk_setting WHERE name = 'default'");
  return res;
};

template <ticketeer::db::Db DB>
[[nodiscard]] crow::response AssignTickets(const crow::request &req, DB &db,
                                           ticketeer::core::Context &) {
  auto body = crow::json::load(req.body);
  if (!body || body.t() != crow::json::type::List) {
    crow::json::wvalue err{{"error", "Expected array of assignments"}};
    return crow::response(400, err);
  }

  if (body.size() == 0) {
    crow::json::wvalue err{{"error", "No assignments provided"}};
    return crow::response(400, err);
  }

  auto sett = GetStatusIds(db);
  if (!sett.ok || sett.nrows() == 0) {
    return crow::response(
        500,
        crow::json::wvalue{{"error", "Failed to retrieve assigned status ID"}});
  }
  auto default_status_id = sett.value(0, 0);
  auto assigned_status_id = sett.value(0, 1);

  auto begin = db.Exec("BEGIN");
  if (!begin.ok) {
    return crow::response(
        503, crow::json::wvalue{{"error", "Failed to start transaction"}});
  }

  int updated = 0;
  std::string ticket_id_str, technician_id_str;
  for (std::size_t i = 0; i < body.size(); ++i) {
    const auto &item = body[i];

    auto ticket_id_opt = internal::GetTicketIdValue(item, ticket_id_str);
    if (!ticket_id_opt)
      continue;

    auto technician_id_opt =
        internal::GetTechnicianIdValue(item, technician_id_str);
    if (!technician_id_opt)
      continue;

    auto ticket_id = ticket_id_opt.value();
    auto technician_id = technician_id_opt.value();

    const char *status_id =
        technician_id ? assigned_status_id : default_status_id;

    const char *params[] = {ticket_id, technician_id, status_id};
    auto res = db.ExecParams("UPDATE ticketeer.helpdesk_ticket "
                             "SET assigned_to_id = $2::bigint,"
                             "    status_id = $3::bigint,"
                             "    updated_at = now() "
                             "WHERE id = $1::bigint "
                             "RETURNING id",
                             params, 3);
    if (!res.ok) {
      auto rollback = db.Exec("ROLLBACK");
      return crow::response(503, res.error);
    }
    if (res.ok && res.nrows() > 0) {
      ++updated;
    }
  }

  auto commit = db.Exec("COMMIT");
  if (!commit.ok) {
    auto rollback = db.Exec("ROLLBACK");
    return crow::response(
        503, crow::json::wvalue{{"error", "Failed to commit transaction"}});
  }

  crow::json::wvalue out{{"updated", updated}};
  return crow::response(200, out);
}

} // namespace ticketeer::apps::helpdesk::roles::supervisor::handlers
