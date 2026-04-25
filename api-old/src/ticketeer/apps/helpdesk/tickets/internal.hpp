#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include <crow_all.h>

#include "ticketeer/core/context.hpp"
#include "ticketeer/core/pagination.hpp"
#include "ticketeer/db/base.hpp"

namespace ticketeer::apps::helpdesk::tickets {

template <ticketeer::db::DbResult R>
static inline crow::response BuildPagedTickets(const R &res, int limit) {
  const int count = res.nrows();
  const bool has_more = count > limit;
  const int n = has_more ? limit : count;

  crow::json::wvalue out;
  for (int i = 0; i < n; ++i) {
    out["items"][i]["id"] = std::stoll(res.value(i, 0));
    out["items"][i]["request_type_id"] = std::stoll(res.value(i, 1));
    out["items"][i]["requester_id"] = std::stoll(res.value(i, 2));
    out["items"][i]["assigned_to_id"] =
        std::string_view{res.value(i, 3)}.empty()
            ? crow::json::wvalue{}
            : crow::json::wvalue{
                  static_cast<std::int64_t>(std::stoll(res.value(i, 3)))};
    out["items"][i]["department_id"] = std::stoll(res.value(i, 4));
    out["items"][i]["priority_id"] = std::stoll(res.value(i, 5));
    out["items"][i]["status_id"] = std::stoll(res.value(i, 6));
    out["items"][i]["description"] = res.value(i, 7);
    out["items"][i]["due_date"] = std::string_view{res.value(i, 8)}.empty()
                                      ? crow::json::wvalue{}
                                      : crow::json::wvalue{res.value(i, 8)};
    out["items"][i]["created_at"] = res.value(i, 9);
    out["items"][i]["updated_at"] = res.value(i, 10);
  }
  out["next"] = has_more
                    ? crow::json::wvalue{ticketeer::core::EncodeCursor(
                          std::stoll(res.value(n - 1, 0)), res.value(n - 1, 9))}
                    : crow::json::wvalue{};
  return crow::response(200, out);
}

template <ticketeer::db::Db DB>
[[nodiscard]] crow::response ListTickets(const crow::request &req, DB &db,
                                         ticketeer::core::Context &) {
  const auto ks = ticketeer::core::KeysetParams::FromRequest(req);
  const std::string limit_str = std::to_string(ks.limit + 1);

  if (!ks.has_cursor()) {
    const char *params[] = {limit_str.c_str()};
    const auto res =
        db.ExecParams("SELECT t.id, t.request_type_id, t.requester_id,"
                      "       t.assigned_to_id, t.department_id, t.priority_id,"
                      "       t.status_id, t.description, t.due_date,"
                      "       t.created_at, t.updated_at"
                      " FROM ticketeer.helpdesk_ticket t"
                      " ORDER BY t.created_at DESC, t.id DESC"
                      " LIMIT $1",
                      params, 1);
    if (!res.ok)
      return crow::response(503, res.error);
    return BuildPagedTickets(res, ks.limit);
  }

  const auto decoded = ticketeer::core::DecodeCursor(ks.cursor);
  if (!decoded)
    return crow::response(400, "Invalid cursor");

  const std::string cursor_id = std::to_string(decoded->first);
  const std::string cursor_ts = decoded->second;
  const char *params[] = {limit_str.c_str(), cursor_ts.c_str(),
                          cursor_id.c_str()};
  const auto res = db.ExecParams(
      "SELECT t.id, t.request_type_id, t.requester_id,"
      "       t.assigned_to_id, t.department_id, t.priority_id,"
      "       t.status_id, t.description, t.due_date,"
      "       t.created_at, t.updated_at"
      " FROM ticketeer.helpdesk_ticket t"
      " WHERE (t.created_at, t.id) < ($2::timestamptz, $3::bigint)"
      " ORDER BY t.created_at DESC, t.id DESC"
      " LIMIT $1",
      params, 3);
  if (!res.ok)
    return crow::response(503, res.error);
  return BuildPagedTickets(res, ks.limit);
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
[[nodiscard]] crow::response CreateTicket(const crow::request &req, DB &db,
                                          ticketeer::core::Context &) {
  auto body = crow::json::load(req.body);
  if (!body || !body.has("request_type_id") || !body.has("requester_id") ||
      !body.has("department_id") || !body.has("priority_id") ||
      !body.has("description"))
    return crow::response(400, "Invalid request body");

  const std::string request_type_id =
      std::to_string(body["request_type_id"].i());
  const std::string requester_id = std::to_string(body["requester_id"].i());
  const std::string department_id = std::to_string(body["department_id"].i());
  const std::string priority_id = std::to_string(body["priority_id"].i());
  const std::string description = body["description"].s();

  const auto setting =
      db.Exec("SELECT default_status_id FROM ticketeer.helpdesk_setting"
              " WHERE name = 'default'");
  if (!setting.ok)
    return crow::response(503, setting.error);
  if (setting.nrows() == 0)
    return crow::response(503, "Missing default setting");

  const std::string status_id = setting.value(0, 0);

  const char *params[] = {request_type_id.c_str(), requester_id.c_str(),
                          department_id.c_str(),   priority_id.c_str(),
                          status_id.c_str(),       description.c_str()};
  const auto res = db.ExecParams(
      "INSERT INTO ticketeer.helpdesk_ticket"
      " (request_type_id, requester_id, department_id, priority_id,"
      "  status_id, description)"
      " VALUES ($1, $2, $3, $4, $5, $6)"
      " RETURNING id",
      params, 6);
  if (!res.ok)
    return crow::response(503, res.error);

  crow::json::wvalue out;
  out["id"] = std::stoll(res.value(0, 0));
  return crow::response(201, out);
}

template <ticketeer::db::Db DB>
[[nodiscard]] crow::response UpdateTicket(const crow::request &req, DB &db,
                                          ticketeer::core::Context &, int id) {
  auto body = crow::json::load(req.body);
  if (!body || !body.has("request_type_id") || !body.has("department_id") ||
      !body.has("priority_id") || !body.has("status_id") ||
      !body.has("description"))
    return crow::response(400, "Invalid request body");

  const std::string request_type_id =
      std::to_string(body["request_type_id"].i());
  const std::string department_id = std::to_string(body["department_id"].i());
  const std::string priority_id = std::to_string(body["priority_id"].i());
  const std::string status_id = std::to_string(body["status_id"].i());
  const std::string description = body["description"].s();
  const std::string id_str = std::to_string(id);

  std::string assigned_str;
  const char *assigned_ptr = nullptr;
  if (body.has("assigned_to_id") &&
      body["assigned_to_id"].t() != crow::json::type::Null) {
    assigned_str = std::to_string(body["assigned_to_id"].i());
    assigned_ptr = assigned_str.c_str();
  }

  const char *params[] = {request_type_id.c_str(), department_id.c_str(),
                          priority_id.c_str(),     status_id.c_str(),
                          description.c_str(),     assigned_ptr,
                          id_str.c_str()};
  const auto res =
      db.ExecParams("UPDATE ticketeer.helpdesk_ticket"
                    " SET request_type_id = $1, department_id = $2,"
                    "     priority_id = $3, status_id = $4,"
                    "     description = $5, assigned_to_id = $6::bigint,"
                    "     updated_at = now()"
                    " WHERE id = $7",
                    params, 7);
  if (!res.ok)
    return crow::response(503, res.error);

  return crow::response(200);
}

template <ticketeer::db::Db DB>
[[nodiscard]] crow::response PatchTicket(const crow::request &req, DB &db,
                                         ticketeer::core::Context &, int id) {
  auto body = crow::json::load(req.body);
  if (!body)
    return crow::response(400, "Invalid request body");

  std::string rt_str, dept_str, prio_str, status_str, desc_str,
      assigned_val_str;
  const char *rt_ptr = nullptr;
  const char *dept_ptr = nullptr;
  const char *prio_ptr = nullptr;
  const char *status_ptr = nullptr;
  const char *desc_ptr = nullptr;

  if (body.has("request_type_id")) {
    rt_str = std::to_string(body["request_type_id"].i());
    rt_ptr = rt_str.c_str();
  }
  if (body.has("department_id")) {
    dept_str = std::to_string(body["department_id"].i());
    dept_ptr = dept_str.c_str();
  }
  if (body.has("priority_id")) {
    prio_str = std::to_string(body["priority_id"].i());
    prio_ptr = prio_str.c_str();
  }
  if (body.has("status_id")) {
    status_str = std::to_string(body["status_id"].i());
    status_ptr = status_str.c_str();
  }
  if (body.has("description")) {
    desc_str = body["description"].s();
    desc_ptr = desc_str.c_str();
  }

  static constexpr char kTouched[] = "1";
  const char *assigned_touch = nullptr;
  const char *assigned_val = nullptr;
  if (body.has("assigned_to_id")) {
    assigned_touch = kTouched;
    if (body["assigned_to_id"].t() != crow::json::type::Null) {
      assigned_val_str = std::to_string(body["assigned_to_id"].i());
      assigned_val = assigned_val_str.c_str();
    }
  }

  const std::string id_str = std::to_string(id);
  const char *params[] = {rt_ptr,       dept_ptr,      prio_ptr,
                          status_ptr,   desc_ptr,      assigned_touch,
                          assigned_val, id_str.c_str()};
  const auto res = db.ExecParams(
      "UPDATE ticketeer.helpdesk_ticket"
      " SET request_type_id = COALESCE($1::bigint, request_type_id),"
      "     department_id   = COALESCE($2::bigint, department_id),"
      "     priority_id     = COALESCE($3::bigint, priority_id),"
      "     status_id       = COALESCE($4::bigint, status_id),"
      "     description     = COALESCE($5, description),"
      "     assigned_to_id  = CASE WHEN $6::text IS NOT NULL"
      "                            THEN $7::bigint"
      "                            ELSE assigned_to_id END,"
      "     updated_at      = now()"
      " WHERE id = $8",
      params, 8);
  if (!res.ok)
    return crow::response(503, res.error);

  return crow::response(200);
}

template <ticketeer::db::Db DB>
[[nodiscard]] crow::response DeleteTicket(const crow::request &, DB &db,
                                          ticketeer::core::Context &, int id) {
  const std::string id_str = std::to_string(id);
  const char *params[] = {id_str.c_str()};
  const auto res = db.ExecParams(
      "DELETE FROM ticketeer.helpdesk_ticket WHERE id = $1", params, 1);
  if (!res.ok)
    return crow::response(503, res.error);

  return crow::response(204);
}

} // namespace ticketeer::apps::helpdesk::tickets
