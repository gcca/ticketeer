#pragma once

#include <string_view>

#include <crow_all.h>

#include "ticketeer/apps/auth/decorators.hpp"
#include "ticketeer/apps/helpdesk/enums.hpp"
#include "ticketeer/db/base.hpp"

namespace ticketeer::apps::helpdesk::decorators {

template <Role R> constexpr std::string_view RoleName() {
  if constexpr (R == Role::administrator)
    return "administrator";
  else if constexpr (R == Role::supervisor)
    return "supervisor";
  else if constexpr (R == Role::technician)
    return "technician";
  else
    return "requester";
}

template <Role R> struct RoleRequired {
  template <ticketeer::db::Db DB, class Fn>
  static crow::response Call(const crow::request &, DB &db,
                             ticketeer::core::Context &ctx, Fn next) {
    constexpr std::string_view role = RoleName<R>();
    const char *params[] = {ctx.token.data(), role.data()};
    const auto res = db.ExecParams(
        "SELECT 1 FROM ticketeer.auth_session s"
        " JOIN ticketeer.helpdesk_profile hp ON hp.user_id = s.user_id"
        " WHERE s.token = $1 AND hp.role = $2::ticketeer.helpdesk_role",
        params, 2);

    if (!res.ok)
      return crow::response(503, res.error);
    if (res.nrows() == 0) {
      crow::json::wvalue err;
      err["error"] = "forbidden";
      return crow::response(403, err);
    }

    return next();
  }
};

} // namespace ticketeer::apps::helpdesk::decorators
