#pragma once

#include <charconv>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include <crow_all.h>
#include <quill/LogMacros.h>

#include "ticketeer/core/context.hpp"
#include "ticketeer/db/base.hpp"

namespace ticketeer::apps::helpdesk::roles::supervisor {

struct TicketAssignment {
  int ticket_id;
  std::optional<int> technician_id;
  std::optional<int> assigned_status_id;
};

template <ticketeer::db::Db DB>
[[nodiscard]] crow::response ListTechnicians(const crow::request &, DB &db,
                                             ticketeer::core::Context &) {
  const auto res = db.Exec(
      "SELECT hp.id, hp.user_id, hp.department_id, hp.role, hp.created_at,"
      "       au.id, au.username, au.created_at"
      " FROM ticketeer.helpdesk_profile hp"
      " JOIN ticketeer.auth_user au ON au.id = hp.user_id"
      " WHERE hp.role = 'technician'"
      " ORDER BY au.username");
  if (!res.ok)
    return crow::response(503, res.error);

  crow::json::wvalue out;
  for (int i = 0; i < res.nrows(); ++i) {
    out[i]["id"] = std::stoll(res.value(i, 0));
    out[i]["user_id"] = std::stoll(res.value(i, 1));
    out[i]["department_id"] = std::stoll(res.value(i, 2));
    out[i]["role"] = res.value(i, 3);
    out[i]["created_at"] = res.value(i, 4);
    out[i]["user"]["id"] = std::stoll(res.value(i, 5));
    out[i]["user"]["username"] = res.value(i, 6);
    out[i]["user"]["created_at"] = res.value(i, 7);
  }
  return crow::response(200, out);
}

template <ticketeer::db::Db DB>
[[nodiscard]] crow::response ListTickets(const crow::request &req, DB &db,
                                         ticketeer::core::Context &) {
  const char *ticket_status_id = req.url_params.get("ticket_status_id");

  crow::json::wvalue out;
  if (!ticket_status_id) {
    const auto res = db.Exec(
        "SELECT t.id, t.description,"
        "       p.display_name AS priority,"
        "       s.display_name AS status,"
        "       t.created_at,"
        "       au.username    AS assigned_to,"
        "       ru.username    AS created_by"
        " FROM ticketeer.helpdesk_ticket t"
        " JOIN ticketeer.helpdesk_priority p ON p.id = t.priority_id"
        " JOIN ticketeer.helpdesk_ticket_status s ON s.id = t.status_id"
        " JOIN ticketeer.helpdesk_profile rp ON rp.id = t.requester_id"
        " JOIN ticketeer.auth_user ru ON ru.id = rp.user_id"
        " LEFT JOIN ticketeer.helpdesk_profile ap ON ap.id = t.assigned_to_id"
        " LEFT JOIN ticketeer.auth_user au ON au.id = ap.user_id"
        " ORDER BY t.created_at DESC"
        " LIMIT 50");
    if (!res.ok)
      return crow::response(503, res.error);

    for (int i = 0; i < res.nrows(); ++i) {
      out[i]["id"] = std::stoll(res.value(i, 0));
      out[i]["description"] = res.value(i, 1);
      out[i]["priority"] = res.value(i, 2);
      out[i]["status"] = res.value(i, 3);
      out[i]["created_at"] = res.value(i, 4);
      out[i]["assigned_to"] = std::string_view{res.value(i, 5)}.empty()
                                  ? crow::json::wvalue{}
                                  : crow::json::wvalue{res.value(i, 5)};
      out[i]["created_by"] = res.value(i, 6);
    }
    return crow::response(200, out);
  }

  const std::string ticket_status_id_str = ticket_status_id;
  long long parsed_status_id = 0;
  const auto [ptr, ec] =
      std::from_chars(ticket_status_id_str.data(),
                      ticket_status_id_str.data() + ticket_status_id_str.size(),
                      parsed_status_id);
  if (ec != std::errc{} ||
      ptr != ticket_status_id_str.data() + ticket_status_id_str.size() ||
      parsed_status_id <= 0)
    return crow::response(400, "Invalid ticket_status_id");

  const char *params[] = {ticket_status_id_str.c_str()};
  const auto res = db.ExecParams(
      "SELECT t.id, t.description,"
      "       p.display_name AS priority,"
      "       s.display_name AS status,"
      "       t.created_at,"
      "       au.username    AS assigned_to,"
      "       ru.username    AS created_by"
      " FROM ticketeer.helpdesk_ticket t"
      " JOIN ticketeer.helpdesk_priority p ON p.id = t.priority_id"
      " JOIN ticketeer.helpdesk_ticket_status s ON s.id = t.status_id"
      " JOIN ticketeer.helpdesk_profile rp ON rp.id = t.requester_id"
      " JOIN ticketeer.auth_user ru ON ru.id = rp.user_id"
      " LEFT JOIN ticketeer.helpdesk_profile ap ON ap.id = t.assigned_to_id"
      " LEFT JOIN ticketeer.auth_user au ON au.id = ap.user_id"
      " WHERE t.status_id = $1::bigint"
      " ORDER BY t.created_at DESC"
      " LIMIT 50",
      params, 1);
  if (!res.ok)
    return crow::response(503, res.error);

  for (int i = 0; i < res.nrows(); ++i) {
    out[i]["id"] = std::stoll(res.value(i, 0));
    out[i]["description"] = res.value(i, 1);
    out[i]["priority"] = res.value(i, 2);
    out[i]["status"] = res.value(i, 3);
    out[i]["created_at"] = res.value(i, 4);
    out[i]["assigned_to"] = std::string_view{res.value(i, 5)}.empty()
                                ? crow::json::wvalue{}
                                : crow::json::wvalue{res.value(i, 5)};
    out[i]["created_by"] = res.value(i, 6);
  }
  return crow::response(200, out);
}

template <ticketeer::db::Db DB>
[[nodiscard]] crow::response GetTicket(const crow::request &, DB &db,
                                       ticketeer::core::Context &, int id) {
  const std::string id_str = std::to_string(id);
  const char *params[] = {id_str.c_str()};
  const auto res =
      db.ExecParams("SELECT t.id, t.request_type_id, t.requester_id,"
                    "       t.assigned_to_id, t.department_id, t.priority_id,"
                    "       t.status_id, t.description, t.due_date,"
                    "       t.created_at, t.updated_at"
                    " FROM ticketeer.helpdesk_ticket t"
                    " WHERE t.id = $1",
                    params, 1);
  if (!res.ok)
    return crow::response(503, res.error);
  if (res.nrows() == 0) {
    crow::json::wvalue err;
    err["error"] = "not_found";
    return crow::response(404, err);
  }

  crow::json::wvalue out;
  out["id"] = std::stoll(res.value(0, 0));
  out["request_type_id"] = std::stoll(res.value(0, 1));
  out["requester_id"] = std::stoll(res.value(0, 2));
  out["assigned_to_id"] = std::string_view{res.value(0, 3)}.empty()
                              ? crow::json::wvalue{}
                              : crow::json::wvalue{static_cast<std::int64_t>(
                                    std::stoll(res.value(0, 3)))};
  out["department_id"] = std::stoll(res.value(0, 4));
  out["priority_id"] = std::stoll(res.value(0, 5));
  out["status_id"] = std::stoll(res.value(0, 6));
  out["description"] = res.value(0, 7);
  out["due_date"] = std::string_view{res.value(0, 8)}.empty()
                        ? crow::json::wvalue{}
                        : crow::json::wvalue{res.value(0, 8)};
  out["created_at"] = res.value(0, 9);
  out["updated_at"] = res.value(0, 10);
  return crow::response(200, out);
}

template <ticketeer::db::Db DB>
[[nodiscard]] crow::response UpdateTicket(const crow::request &req, DB &db,
                                          ticketeer::core::Context &, int id) {
  auto body = crow::json::load(req.body);
  if (!body || !body.has("request_type_id") || !body.has("requester_id") ||
      !body.has("assigned_to_id") || !body.has("department_id") ||
      !body.has("priority_id") || !body.has("status_id") ||
      !body.has("description") || !body.has("due_date"))
    return crow::response(400, "Invalid request body");

  const std::string request_type_id =
      std::to_string(body["request_type_id"].i());
  const std::string requester_id = std::to_string(body["requester_id"].i());
  const std::string department_id = std::to_string(body["department_id"].i());
  const std::string priority_id = std::to_string(body["priority_id"].i());
  const std::string status_id = std::to_string(body["status_id"].i());
  const std::string description = body["description"].s();
  const std::string id_str = std::to_string(id);

  std::string assigned_str;
  const char *assigned_ptr = nullptr;
  if (body["assigned_to_id"].t() != crow::json::type::Null) {
    const int assigned_to_id = static_cast<int>(body["assigned_to_id"].i());
    if (assigned_to_id <= 0)
      return crow::response(400, "Invalid request body");

    assigned_str = std::to_string(assigned_to_id);
    assigned_ptr = assigned_str.c_str();

    const char *technician_params[] = {assigned_ptr};
    const auto technician =
        db.ExecParams("SELECT 1 FROM ticketeer.helpdesk_profile"
                      " WHERE id = $1 AND role = 'technician'",
                      technician_params, 1);
    if (!technician.ok)
      return crow::response(503, technician.error);
    if (technician.nrows() == 0) {
      crow::json::wvalue err;
      err["error"] = "invalid_technician";
      err["technician_id"] = assigned_to_id;
      return crow::response(400, err);
    }
  }

  std::string due_date_str;
  const char *due_date_ptr = nullptr;
  if (body["due_date"].t() != crow::json::type::Null) {
    due_date_str = body["due_date"].s();
    due_date_ptr = due_date_str.c_str();
  }

  const char *params[] = {
      request_type_id.c_str(), requester_id.c_str(), assigned_ptr,
      department_id.c_str(),   priority_id.c_str(),  status_id.c_str(),
      description.c_str(),     due_date_ptr,         id_str.c_str()};
  const auto res = db.ExecParams(
      "UPDATE ticketeer.helpdesk_ticket"
      " SET request_type_id = $1::bigint, requester_id = $2::bigint,"
      "     assigned_to_id = $3::bigint, department_id = $4::bigint,"
      "     priority_id = $5::bigint,"
      "     status_id = CASE"
      "         WHEN assigned_to_id IS NULL AND $3::bigint IS NOT NULL THEN"
      "             (SELECT assigned_status_id FROM ticketeer.helpdesk_setting"
      "              WHERE name = 'default')"
      "         ELSE $6::bigint"
      "     END,"
      "     description = $7, due_date = $8::timestamptz,"
      "     updated_at = now()"
      " WHERE id = $9::bigint"
      " RETURNING id",
      params, 9);
  if (!res.ok)
    return crow::response(503, res.error);
  if (res.nrows() == 0) {
    crow::json::wvalue err;
    err["error"] = "not_found";
    return crow::response(404, err);
  }

  return crow::response(200);
}

} // namespace ticketeer::apps::helpdesk::roles::supervisor
