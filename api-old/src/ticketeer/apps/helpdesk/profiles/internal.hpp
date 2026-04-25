#pragma once

#include <crow_all.h>

#include "ticketeer/core/context.hpp"
#include "ticketeer/db/base.hpp"

namespace ticketeer::apps::helpdesk::profiles {

template <ticketeer::db::Db DB>
[[nodiscard]] crow::response GetMyProfile(const crow::request &, DB &db,
                                          ticketeer::core::Context &ctx) {
  const char *session_params[] = {ctx.token.data()};
  const auto session = db.ExecParams("SELECT user_id FROM ticketeer.auth_session"
                                     " WHERE token = $1 AND expires_at > now()",
                                     session_params, 1);
  if (!session.ok)
    return crow::response(503, session.error);

  const std::string user_id = session.value(0, 0);

  const char *profile_params[] = {user_id.c_str()};
  const auto res =
      db.ExecParams("SELECT id, user_id, department_id, role, created_at"
                    " FROM ticketeer.helpdesk_profile"
                    " WHERE user_id = $1",
                    profile_params, 1);
  if (!res.ok)
    return crow::response(503, res.error);
  if (res.nrows() == 0) {
    crow::json::wvalue err;
    err["error"] = "not_found";
    return crow::response(404, err);
  }

  crow::json::wvalue out;
  out["id"] = std::stoll(res.value(0, 0));
  out["user_id"] = std::stoll(res.value(0, 1));
  out["department_id"] = std::stoll(res.value(0, 2));
  out["role"] = res.value(0, 3);
  out["created_at"] = res.value(0, 4);
  return crow::response(200, out);
}

} // namespace ticketeer::apps::helpdesk::profiles
