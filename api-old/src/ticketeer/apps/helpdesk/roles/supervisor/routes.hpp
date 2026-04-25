#pragma once

#include <crow_all.h>

#include "handlers/assign-tickets.hpp"
#include "internal.hpp"
#include "ticketeer/apps/auth/decorators.hpp"
#include "ticketeer/apps/helpdesk/decorators.hpp"
#include "ticketeer/core/routes.hpp"
#include "ticketeer/db/pg.hpp"

namespace ticketeer::apps::helpdesk::roles::supervisor {

inline void RegisterRoutes(crow::SimpleApp &app) {
  using auth::decorators::LoginRequired;
  using helpdesk::Role;
  using helpdesk::decorators::RoleRequired;
  namespace cr = ticketeer::core;

  CROW_ROUTE(app, "/ticketeer/api/v1/helpdesk/roles/supervisor/tickets")
      .methods(crow::HTTPMethod::GET)(
          cr::Handler<LoginRequired, RoleRequired<Role::supervisor>>(
              ListTickets<ticketeer::db::Pg>));

  CROW_ROUTE(app, "/ticketeer/api/v1/helpdesk/roles/supervisor/tickets/<int>")
      .methods(crow::HTTPMethod::GET)(
          cr::HandlerId<LoginRequired, RoleRequired<Role::supervisor>>(
              GetTicket<ticketeer::db::Pg>));

  CROW_ROUTE(app, "/ticketeer/api/v1/helpdesk/roles/supervisor/tickets/<int>")
      .methods(crow::HTTPMethod::PUT)(
          cr::HandlerId<LoginRequired, RoleRequired<Role::supervisor>>(
              UpdateTicket<ticketeer::db::Pg>));

  CROW_ROUTE(app, "/ticketeer/api/v1/helpdesk/roles/supervisor/assign-tickets")
      .methods(crow::HTTPMethod::POST)(
          cr::Handler<LoginRequired, RoleRequired<Role::supervisor>>(
              handlers::AssignTickets<ticketeer::db::Pg>));

  CROW_ROUTE(app, "/ticketeer/api/v1/helpdesk/roles/supervisor/technicians")
      .methods(crow::HTTPMethod::GET)(
          cr::Handler<LoginRequired, RoleRequired<Role::supervisor>>(
              ListTechnicians<ticketeer::db::Pg>));
}

} // namespace ticketeer::apps::helpdesk::roles::supervisor
