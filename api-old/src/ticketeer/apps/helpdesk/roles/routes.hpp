#pragma once

#include <crow_all.h>

#include "internal.hpp"
#include "ticketeer/apps/auth/decorators.hpp"
#include "ticketeer/core/routes.hpp"
#include "ticketeer/db/pg.hpp"

namespace ticketeer::apps::helpdesk::roles {

inline void RegisterRoutes(crow::SimpleApp &app) {
  using auth::decorators::LoginRequired;
  namespace cr = ticketeer::core;

  CROW_ROUTE(app, "/ticketeer/api/v1/helpdesk/roles/content")
      .methods(crow::HTTPMethod::GET)(
          cr::Handler<LoginRequired>(RolesContent<ticketeer::db::Pg>));
}

} // namespace ticketeer::apps::helpdesk::roles
