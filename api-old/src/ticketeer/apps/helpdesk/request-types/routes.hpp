#pragma once

#include <crow_all.h>

#include "internal.hpp"
#include "ticketeer/apps/helpdesk/decorators.hpp"
#include "ticketeer/core/routes.hpp"
#include "ticketeer/db/pg.hpp"

namespace ticketeer::apps::helpdesk::request_types {

inline void RegisterRoutes(crow::SimpleApp &app) {
  using auth::decorators::LoginRequired;
  using helpdesk::Role;
  using helpdesk::decorators::RoleRequired;
  namespace cr = ticketeer::core;

  CROW_ROUTE(app, "/ticketeer/api/v1/helpdesk/request-types")
      .methods(crow::HTTPMethod::GET)(
          cr::Handler<LoginRequired, RoleRequired<Role::administrator>>(
              ListRequestTypes<ticketeer::db::Pg>));

  CROW_ROUTE(app, "/ticketeer/api/v1/helpdesk/request-types")
      .methods(crow::HTTPMethod::POST)(
          cr::Handler<LoginRequired, RoleRequired<Role::administrator>>(
              CreateRequestType<ticketeer::db::Pg>));

  CROW_ROUTE(app, "/ticketeer/api/v1/helpdesk/request-types/<int>")
      .methods(crow::HTTPMethod::GET)(
          cr::HandlerId<LoginRequired, RoleRequired<Role::administrator>>(
              GetRequestType<ticketeer::db::Pg>));

  CROW_ROUTE(app, "/ticketeer/api/v1/helpdesk/request-types/<int>")
      .methods(crow::HTTPMethod::PUT)(
          cr::HandlerId<LoginRequired, RoleRequired<Role::administrator>>(
              UpdateRequestType<ticketeer::db::Pg>));

  CROW_ROUTE(app, "/ticketeer/api/v1/helpdesk/request-types/<int>")
      .methods(crow::HTTPMethod::PATCH)(
          cr::HandlerId<LoginRequired, RoleRequired<Role::administrator>>(
              PatchRequestType<ticketeer::db::Pg>));

  CROW_ROUTE(app, "/ticketeer/api/v1/helpdesk/request-types/<int>")
      .methods(crow::HTTPMethod::Delete)(
          cr::HandlerId<LoginRequired, RoleRequired<Role::administrator>>(
              DeleteRequestType<ticketeer::db::Pg>));
}

} // namespace ticketeer::apps::helpdesk::request_types
