#pragma once

#include <crow_all.h>

#include "internal.hpp"
#include "ticketeer/apps/helpdesk/decorators.hpp"
#include "ticketeer/core/routes.hpp"
#include "ticketeer/db/pg.hpp"

namespace ticketeer::apps::helpdesk::priorities {

inline void RegisterRoutes(crow::SimpleApp &app) {
  using auth::decorators::LoginRequired;
  using helpdesk::Role;
  using helpdesk::decorators::RoleRequired;
  namespace cr = ticketeer::core;

  CROW_ROUTE(app, "/ticketeer/api/v1/helpdesk/priorities")
      .methods(crow::HTTPMethod::GET)(
          cr::Handler<LoginRequired, RoleRequired<Role::administrator>>(
              ListPriorities<ticketeer::db::Pg>));

  CROW_ROUTE(app, "/ticketeer/api/v1/helpdesk/priorities")
      .methods(crow::HTTPMethod::POST)(
          cr::Handler<LoginRequired, RoleRequired<Role::administrator>>(
              CreatePriority<ticketeer::db::Pg>));

  CROW_ROUTE(app, "/ticketeer/api/v1/helpdesk/priorities/<int>")
      .methods(crow::HTTPMethod::GET)(
          cr::HandlerId<LoginRequired, RoleRequired<Role::administrator>>(
              GetPriority<ticketeer::db::Pg>));

  CROW_ROUTE(app, "/ticketeer/api/v1/helpdesk/priorities/<int>")
      .methods(crow::HTTPMethod::PUT)(
          cr::HandlerId<LoginRequired, RoleRequired<Role::administrator>>(
              UpdatePriority<ticketeer::db::Pg>));

  CROW_ROUTE(app, "/ticketeer/api/v1/helpdesk/priorities/<int>")
      .methods(crow::HTTPMethod::PATCH)(
          cr::HandlerId<LoginRequired, RoleRequired<Role::administrator>>(
              PatchPriority<ticketeer::db::Pg>));

  CROW_ROUTE(app, "/ticketeer/api/v1/helpdesk/priorities/<int>")
      .methods(crow::HTTPMethod::Delete)(
          cr::HandlerId<LoginRequired, RoleRequired<Role::administrator>>(
              DeletePriority<ticketeer::db::Pg>));
}

} // namespace ticketeer::apps::helpdesk::priorities
