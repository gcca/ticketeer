#pragma once

#include <optional>
#include <string>

#include <crow_all.h>

#include "ticketeer/core/context.hpp"
#include "ticketeer/db/base.hpp"

namespace ticketeer::apps::helpdesk::roles {

template <ticketeer::db::Db DB>
static inline std::optional<crow::response>
FetchProfile(DB &db, const std::string &user_id, crow::json::wvalue &out) {
  const char *params[] = {user_id.c_str()};
  const auto res =
      db.ExecParams("SELECT id, user_id, department_id, role, created_at"
                    " FROM ticketeer.helpdesk_profile"
                    " WHERE user_id = $1",
                    params, 1);
  if (!res.ok)
    return crow::response(503, res.error);
  if (res.nrows() == 0) {
    crow::json::wvalue err;
    err["error"] = "not_found";
    return crow::response(404, err);
  }

  out["id"] = std::stoll(res.value(0, 0));
  out["user_id"] = std::stoll(res.value(0, 1));
  out["department_id"] = std::stoll(res.value(0, 2));
  out["role"] = res.value(0, 3);
  out["created_at"] = res.value(0, 4);
  return std::nullopt;
}

template <ticketeer::db::Db DB>
static inline std::optional<crow::response>
FetchDepartments(DB &db, crow::json::wvalue &out) {
  const auto res = db.Exec(
      "SELECT id, name FROM ticketeer.helpdesk_department ORDER BY name");
  if (!res.ok)
    return crow::response(503, res.error);

  for (int i = 0; i < res.nrows(); ++i) {
    out[i]["id"] = std::stoll(res.value(i, 0));
    out[i]["name"] = res.value(i, 1);
  }
  return std::nullopt;
}

template <ticketeer::db::Db DB>
static inline std::optional<crow::response>
FetchPriorities(DB &db, crow::json::wvalue &out) {
  const auto res = db.Exec("SELECT id, name, display_name FROM "
                           "ticketeer.helpdesk_priority ORDER BY id");
  if (!res.ok)
    return crow::response(503, res.error);

  for (int i = 0; i < res.nrows(); ++i) {
    out[i]["id"] = std::stoll(res.value(i, 0));
    out[i]["name"] = res.value(i, 1);
    out[i]["display_name"] = res.value(i, 2);
  }
  return std::nullopt;
}

template <ticketeer::db::Db DB>
static inline std::optional<crow::response>
FetchRequestTypes(DB &db, crow::json::wvalue &out) {
  const auto res =
      db.Exec("SELECT id, name, category_id, default_priority_id, description"
              " FROM ticketeer.helpdesk_request_type ORDER BY name");
  if (!res.ok)
    return crow::response(503, res.error);

  for (int i = 0; i < res.nrows(); ++i) {
    out[i]["id"] = std::stoll(res.value(i, 0));
    out[i]["name"] = res.value(i, 1);
    out[i]["category_id"] = std::stoll(res.value(i, 2));
    out[i]["default_priority_id"] = std::stoll(res.value(i, 3));
    out[i]["description"] = res.value(i, 4);
  }
  return std::nullopt;
}

template <ticketeer::db::Db DB>
static inline std::optional<crow::response>
FetchTicketStatuses(DB &db, crow::json::wvalue &out) {
  const auto res = db.Exec("SELECT id, name, display_name, trait FROM "
                           "ticketeer.helpdesk_ticket_status ORDER BY id");
  if (!res.ok)
    return crow::response(503, res.error);

  for (int i = 0; i < res.nrows(); ++i) {
    out[i]["id"] = std::stoll(res.value(i, 0));
    out[i]["name"] = res.value(i, 1);
    out[i]["display_name"] = res.value(i, 2);
    out[i]["trait"] = res.value(i, 3);
  }
  return std::nullopt;
}

template <ticketeer::db::Db DB>
[[nodiscard]] crow::response RolesContent(const crow::request &, DB &db,
                                          ticketeer::core::Context &ctx) {
  const char *session_params[] = {ctx.token.data()};
  const auto session = db.ExecParams("SELECT user_id FROM ticketeer.auth_session"
                                     " WHERE token = $1 AND expires_at > now()",
                                     session_params, 1);
  if (!session.ok)
    return crow::response(503, session.error);

  const std::string user_id = session.value(0, 0);

  crow::json::wvalue out;
  if (auto err = FetchProfile(db, user_id, out["profile"]))
    return std::move(*err);
  if (auto err = FetchDepartments(db, out["departments"]))
    return std::move(*err);
  if (auto err = FetchPriorities(db, out["priorities"]))
    return std::move(*err);
  if (auto err = FetchRequestTypes(db, out["request_types"]))
    return std::move(*err);
  if (auto err = FetchTicketStatuses(db, out["ticket_statuses"]))
    return std::move(*err);
  return crow::response(200, out);
}

} // namespace ticketeer::apps::helpdesk::roles
