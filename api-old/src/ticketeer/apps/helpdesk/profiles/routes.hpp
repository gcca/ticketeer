#pragma once

#include <crow_all.h>

#include "internal.hpp"
#include "ticketeer/apps/auth/decorators.hpp"
#include "ticketeer/core/routes.hpp"
#include "ticketeer/db/pg.hpp"

namespace ticketeer::apps::helpdesk::profiles {

inline void RegisterRoutes(crow::SimpleApp &app) {
  using auth::decorators::LoginRequired;
  namespace cr = ticketeer::core;

  CROW_ROUTE(app, "/ticketeer/api/v1/helpdesk/profiles/me")
      .methods(crow::HTTPMethod::GET)(
          cr::Handler<LoginRequired>(GetMyProfile<ticketeer::db::Pg>));
}

} // namespace ticketeer::apps::helpdesk::profiles
