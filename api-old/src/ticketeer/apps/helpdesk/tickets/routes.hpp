#pragma once

#include <crow_all.h>

#include "internal.hpp"
#include "ticketeer/apps/helpdesk/decorators.hpp"
#include "ticketeer/core/routes.hpp"

namespace ticketeer::apps::helpdesk::tickets {

inline void RegisterRoutes(crow::SimpleApp &app) {
  using auth::decorators::LoginRequired;
  using helpdesk::Role;
  using helpdesk::decorators::RoleRequired;
  namespace cr = ticketeer::core;

  CROW_ROUTE(app, "/ticketeer/api/v1/helpdesk/tickets")
      .methods(crow::HTTPMethod::GET)(
          cr::Handler<LoginRequired, RoleRequired<Role::administrator>>(
              ListTickets<ticketeer::db::Pg>));

  CROW_ROUTE(app, "/ticketeer/api/v1/helpdesk/tickets")
      .methods(crow::HTTPMethod::POST)(
          cr::Handler<LoginRequired, RoleRequired<Role::administrator>>(
              CreateTicket<ticketeer::db::Pg>));

  CROW_ROUTE(app, "/ticketeer/api/v1/helpdesk/tickets/<int>")
      .methods(crow::HTTPMethod::GET)(
          cr::HandlerId<LoginRequired, RoleRequired<Role::administrator>>(
              GetTicket<ticketeer::db::Pg>));

  CROW_ROUTE(app, "/ticketeer/api/v1/helpdesk/tickets/<int>")
      .methods(crow::HTTPMethod::PUT)(
          cr::HandlerId<LoginRequired, RoleRequired<Role::administrator>>(
              UpdateTicket<ticketeer::db::Pg>));

  CROW_ROUTE(app, "/ticketeer/api/v1/helpdesk/tickets/<int>")
      .methods(crow::HTTPMethod::PATCH)(
          cr::HandlerId<LoginRequired, RoleRequired<Role::administrator>>(
              PatchTicket<ticketeer::db::Pg>));

  CROW_ROUTE(app, "/ticketeer/api/v1/helpdesk/tickets/<int>")
      .methods(crow::HTTPMethod::Delete)(
          cr::HandlerId<LoginRequired, RoleRequired<Role::administrator>>(
              DeleteTicket<ticketeer::db::Pg>));
}

} // namespace ticketeer::apps::helpdesk::tickets
