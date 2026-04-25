#pragma once

#include <cstdint>

#include <crow_all.h>

#include "ticketeer/core/context.hpp"
#include "ticketeer/core/pagination.hpp"
#include "ticketeer/db/base.hpp"

namespace ticketeer::apps::helpdesk::roles::requester {

template <ticketeer::db::Db DB>
[[nodiscard]] crow::response RequestTicket(const crow::request &req, DB &db,
                                           ticketeer::core::Context &ctx) {
  auto body = crow::json::load(req.body);
  if (!body || !body.has("request_type_id") || !body.has("department_id") ||
      !body.has("priority_id") || !body.has("description"))
    return crow::response(400, "Invalid request body");

  const std::string request_type_id =
      std::to_string(body["request_type_id"].i());
  const std::string department_id = std::to_string(body["department_id"].i());
  const std::string priority_id = std::to_string(body["priority_id"].i());
  const std::string description = body["description"].s();

  const char *session_params[] = {ctx.token.data()};
  const auto session = db.ExecParams("SELECT user_id FROM ticketeer.auth_session"
                                     " WHERE token = $1 AND expires_at > now()",
                                     session_params, 1);
  if (!session.ok)
    return crow::response(503, session.error);

  const std::string user_id = session.value(0, 0);

  const char *profile_params[] = {user_id.c_str()};
  const auto profile_res = db.ExecParams(
      "SELECT id FROM ticketeer.helpdesk_profile WHERE user_id = $1",
      profile_params, 1);
  if (!profile_res.ok)
    return crow::response(503, profile_res.error);

  std::string profile_id;
  if (profile_res.nrows() == 0) {
    const char *create_params[] = {user_id.c_str(), department_id.c_str()};
    const auto new_profile = db.ExecParams(
        "INSERT INTO ticketeer.helpdesk_profile (user_id, department_id, role)"
        " VALUES ($1, $2, 'requester') RETURNING id",
        create_params, 2);
    if (!new_profile.ok)
      return crow::response(503, new_profile.error);
    profile_id = new_profile.value(0, 0);
  } else {
    profile_id = profile_res.value(0, 0);
  }

  const auto setting =
      db.Exec("SELECT default_status_id FROM ticketeer.helpdesk_setting"
              " WHERE name = 'default'");
  if (!setting.ok)
    return crow::response(503, setting.error);
  if (setting.nrows() == 0)
    return crow::response(503, "Missing default setting");

  const std::string status_id = setting.value(0, 0);

  const char *ticket_params[] = {request_type_id.c_str(), profile_id.c_str(),
                                 department_id.c_str(),   priority_id.c_str(),
                                 status_id.c_str(),       description.c_str()};
  const auto ticket = db.ExecParams(
      "INSERT INTO ticketeer.helpdesk_ticket"
      " (request_type_id, requester_id, department_id, priority_id,"
      "  status_id, description)"
      " VALUES ($1, $2, $3, $4, $5, $6)"
      " RETURNING id",
      ticket_params, 6);
  if (!ticket.ok)
    return crow::response(503, ticket.error);

  crow::json::wvalue out;
  out["id"] = std::stoll(ticket.value(0, 0));
  return crow::response(201, out);
}

template <ticketeer::db::Db DB>
[[nodiscard]] crow::response ListRequestedBy(const crow::request &req, DB &db,
                                             ticketeer::core::Context &ctx,
                                             int profile_id) {
  const std::string profile_id_str = std::to_string(profile_id);

  const char *profile_params[] = {profile_id_str.c_str()};
  const auto profile_res = db.ExecParams(
      "SELECT user_id FROM ticketeer.helpdesk_profile WHERE id = $1",
      profile_params, 1);
  if (!profile_res.ok)
    return crow::response(503, profile_res.error);
  if (profile_res.nrows() == 0) {
    crow::json::wvalue err;
    err["error"] = "not_found";
    return crow::response(404, err);
  }

  const std::string profile_user_id = profile_res.value(0, 0);

  const char *session_params[] = {ctx.token.data()};
  const auto session = db.ExecParams("SELECT user_id FROM ticketeer.auth_session"
                                     " WHERE token = $1 AND expires_at > now()",
                                     session_params, 1);
  if (!session.ok)
    return crow::response(503, session.error);

  if (std::string_view{session.value(0, 0)} != profile_user_id) {
    crow::json::wvalue err;
    err["error"] = "forbidden";
    return crow::response(403, err);
  }

  const auto ks = ticketeer::core::KeysetParams::FromRequest(req);
  const std::string limit_str = std::to_string(ks.limit + 1);

  if (!ks.has_cursor()) {
    const char *params[] = {profile_id_str.c_str(), limit_str.c_str()};
    const auto res = db.ExecParams(
        "SELECT id, request_type_id, assigned_to_id, department_id,"
        "       priority_id, status_id, description, due_date,"
        "       created_at, updated_at"
        " FROM ticketeer.helpdesk_ticket"
        " WHERE requester_id = $1"
        " ORDER BY created_at DESC, id DESC"
        " LIMIT $2",
        params, 2);
    if (!res.ok)
      return crow::response(503, res.error);

    const int count = res.nrows();
    const bool has_more = count > ks.limit;
    const int n = has_more ? ks.limit : count;
    crow::json::wvalue out;
    for (int i = 0; i < n; ++i) {
      out["items"][i]["id"] = std::stoll(res.value(i, 0));
      out["items"][i]["request_type_id"] = std::stoll(res.value(i, 1));
      out["items"][i]["assigned_to_id"] =
          std::string_view{res.value(i, 2)}.empty()
              ? crow::json::wvalue{}
              : crow::json::wvalue{
                    static_cast<std::int64_t>(std::stoll(res.value(i, 2)))};
      out["items"][i]["department_id"] = std::stoll(res.value(i, 3));
      out["items"][i]["priority_id"] = std::stoll(res.value(i, 4));
      out["items"][i]["status_id"] = std::stoll(res.value(i, 5));
      out["items"][i]["description"] = res.value(i, 6);
      out["items"][i]["due_date"] = std::string_view{res.value(i, 7)}.empty()
                                        ? crow::json::wvalue{}
                                        : crow::json::wvalue{res.value(i, 7)};
      out["items"][i]["created_at"] = res.value(i, 8);
      out["items"][i]["updated_at"] = res.value(i, 9);
    }
    out["next"] =
        has_more ? crow::json::wvalue{ticketeer::core::EncodeCursor(
                       std::stoll(res.value(n - 1, 0)), res.value(n - 1, 8))}
                 : crow::json::wvalue{};
    return crow::response(200, out);
  }

  const auto decoded = ticketeer::core::DecodeCursor(ks.cursor);
  if (!decoded)
    return crow::response(400, "Invalid cursor");

  const std::string cursor_id = std::to_string(decoded->first);
  const std::string cursor_ts = decoded->second;
  const char *params[] = {profile_id_str.c_str(), limit_str.c_str(),
                          cursor_ts.c_str(), cursor_id.c_str()};
  const auto res =
      db.ExecParams("SELECT id, request_type_id, assigned_to_id, department_id,"
                    "       priority_id, status_id, description, due_date,"
                    "       created_at, updated_at"
                    " FROM ticketeer.helpdesk_ticket"
                    " WHERE requester_id = $1"
                    "   AND (created_at, id) < ($3::timestamptz, $4::bigint)"
                    " ORDER BY created_at DESC, id DESC"
                    " LIMIT $2",
                    params, 4);
  if (!res.ok)
    return crow::response(503, res.error);

  const int count = res.nrows();
  const bool has_more = count > ks.limit;
  const int n = has_more ? ks.limit : count;
  crow::json::wvalue out;
  for (int i = 0; i < n; ++i) {
    out["items"][i]["id"] = std::stoll(res.value(i, 0));
    out["items"][i]["request_type_id"] = std::stoll(res.value(i, 1));
    out["items"][i]["assigned_to_id"] =
        std::string_view{res.value(i, 2)}.empty()
            ? crow::json::wvalue{}
            : crow::json::wvalue{
                  static_cast<std::int64_t>(std::stoll(res.value(i, 2)))};
    out["items"][i]["department_id"] = std::stoll(res.value(i, 3));
    out["items"][i]["priority_id"] = std::stoll(res.value(i, 4));
    out["items"][i]["status_id"] = std::stoll(res.value(i, 5));
    out["items"][i]["description"] = res.value(i, 6);
    out["items"][i]["due_date"] = std::string_view{res.value(i, 7)}.empty()
                                      ? crow::json::wvalue{}
                                      : crow::json::wvalue{res.value(i, 7)};
    out["items"][i]["created_at"] = res.value(i, 8);
    out["items"][i]["updated_at"] = res.value(i, 9);
  }
  out["next"] = has_more
                    ? crow::json::wvalue{ticketeer::core::EncodeCursor(
                          std::stoll(res.value(n - 1, 0)), res.value(n - 1, 8))}
                    : crow::json::wvalue{};
  return crow::response(200, out);
}

} // namespace ticketeer::apps::helpdesk::roles::requester
