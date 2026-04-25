#pragma once

#include <crow_all.h>

#include "internal.hpp"
#include "ticketeer/apps/helpdesk/decorators.hpp"
#include "ticketeer/core/routes.hpp"

namespace ticketeer::apps::helpdesk::departments {

inline void RegisterRoutes(crow::SimpleApp &app) {
  using auth::decorators::LoginRequired;
  using helpdesk::Role;
  using helpdesk::decorators::RoleRequired;
  namespace cr = ticketeer::core;

  CROW_ROUTE(app, "/ticketeer/api/v1/helpdesk/departments")
      .methods(crow::HTTPMethod::GET)(
          cr::Handler<LoginRequired>(ListDepartments<ticketeer::db::Pg>));

  CROW_ROUTE(app, "/ticketeer/api/v1/helpdesk/departments")
      .methods(crow::HTTPMethod::POST)(
          cr::Handler<LoginRequired, RoleRequired<Role::administrator>>(
              CreateDepartment<ticketeer::db::Pg>));

  CROW_ROUTE(app, "/ticketeer/api/v1/helpdesk/departments/<int>")
      .methods(crow::HTTPMethod::GET)(
          cr::HandlerId<LoginRequired>(GetDepartment<ticketeer::db::Pg>));

  CROW_ROUTE(app, "/ticketeer/api/v1/helpdesk/departments/<int>")
      .methods(crow::HTTPMethod::PUT)(
          cr::HandlerId<LoginRequired, RoleRequired<Role::administrator>>(
              UpdateDepartment<ticketeer::db::Pg>));

  CROW_ROUTE(app, "/ticketeer/api/v1/helpdesk/departments/<int>")
      .methods(crow::HTTPMethod::PATCH)(
          cr::HandlerId<LoginRequired, RoleRequired<Role::administrator>>(
              PatchDepartment<ticketeer::db::Pg>));

  CROW_ROUTE(app, "/ticketeer/api/v1/helpdesk/departments/<int>")
      .methods(crow::HTTPMethod::Delete)(
          cr::HandlerId<LoginRequired, RoleRequired<Role::administrator>>(
              DeleteDepartment<ticketeer::db::Pg>));
}

} // namespace ticketeer::apps::helpdesk::departments
