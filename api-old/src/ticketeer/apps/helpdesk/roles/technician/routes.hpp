#pragma once

#include <crow_all.h>

#include "internal.hpp"
#include "ticketeer/apps/auth/decorators.hpp"
#include "ticketeer/apps/helpdesk/decorators.hpp"
#include "ticketeer/core/routes.hpp"
#include "ticketeer/db/pg.hpp"

namespace ticketeer::apps::helpdesk::roles::technician {

inline void RegisterRoutes(crow::SimpleApp &app) {
  using auth::decorators::LoginRequired;
  using helpdesk::Role;
  using helpdesk::decorators::RoleRequired;
  namespace cr = ticketeer::core;

  CROW_ROUTE(app, "/ticketeer/api/v1/helpdesk/roles/technician/tickets")
      .methods(crow::HTTPMethod::GET)(
          cr::Handler<LoginRequired, RoleRequired<Role::technician>>(
              ListTickets<ticketeer::db::Pg>));
}

} // namespace ticketeer::apps::helpdesk::roles::technician
