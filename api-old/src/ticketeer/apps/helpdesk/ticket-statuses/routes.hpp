#pragma once

#include <crow_all.h>

#include "internal.hpp"
#include "ticketeer/apps/helpdesk/decorators.hpp"
#include "ticketeer/core/routes.hpp"
#include "ticketeer/db/pg.hpp"

namespace ticketeer::apps::helpdesk::ticket_statuses {

inline void RegisterRoutes(crow::SimpleApp &app) {
  using auth::decorators::LoginRequired;
  using helpdesk::Role;
  using helpdesk::decorators::RoleRequired;
  namespace cr = ticketeer::core;

  CROW_ROUTE(app, "/ticketeer/api/v1/helpdesk/ticket-statuses")
      .methods(crow::HTTPMethod::GET)(
          cr::Handler<LoginRequired>(ListTicketStatuses<ticketeer::db::Pg>));

  CROW_ROUTE(app, "/ticketeer/api/v1/helpdesk/ticket-statuses")
      .methods(crow::HTTPMethod::POST)(
          cr::Handler<LoginRequired, RoleRequired<Role::administrator>>(
              CreateTicketStatus<ticketeer::db::Pg>));

  CROW_ROUTE(app, "/ticketeer/api/v1/helpdesk/ticket-statuses/<int>")
      .methods(crow::HTTPMethod::GET)(
          cr::HandlerId<LoginRequired, RoleRequired<Role::administrator>>(
              GetTicketStatus<ticketeer::db::Pg>));

  CROW_ROUTE(app, "/ticketeer/api/v1/helpdesk/ticket-statuses/<int>")
      .methods(crow::HTTPMethod::PUT)(
          cr::HandlerId<LoginRequired, RoleRequired<Role::administrator>>(
              UpdateTicketStatus<ticketeer::db::Pg>));

  CROW_ROUTE(app, "/ticketeer/api/v1/helpdesk/ticket-statuses/<int>")
      .methods(crow::HTTPMethod::PATCH)(
          cr::HandlerId<LoginRequired, RoleRequired<Role::administrator>>(
              PatchTicketStatus<ticketeer::db::Pg>));

  CROW_ROUTE(app, "/ticketeer/api/v1/helpdesk/ticket-statuses/<int>")
      .methods(crow::HTTPMethod::Delete)(
          cr::HandlerId<LoginRequired, RoleRequired<Role::administrator>>(
              DeleteTicketStatus<ticketeer::db::Pg>));
}

} // namespace ticketeer::apps::helpdesk::ticket_statuses
