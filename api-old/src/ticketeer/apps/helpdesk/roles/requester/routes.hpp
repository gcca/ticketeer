#pragma once

#include <crow_all.h>

#include "internal.hpp"
#include "ticketeer/apps/helpdesk/decorators.hpp"
#include "ticketeer/core/routes.hpp"

namespace ticketeer::apps::helpdesk::roles::requester {

inline void RegisterRoutes(crow::SimpleApp &app) {
  using auth::decorators::LoginRequired;
  using helpdesk::Role;
  using helpdesk::decorators::RoleRequired;
  namespace cr = ticketeer::core;

  CROW_ROUTE(app, "/ticketeer/api/v1/helpdesk/roles/requester/request")
      .methods(crow::HTTPMethod::POST)(
          cr::Handler<LoginRequired>(RequestTicket<ticketeer::db::Pg>));

  CROW_ROUTE(app,
             "/ticketeer/api/v1/helpdesk/roles/requester/requested_by/<int>")
      .methods(crow::HTTPMethod::GET)(
          cr::HandlerId<LoginRequired>(ListRequestedBy<ticketeer::db::Pg>));
}
} // namespace ticketeer::apps::helpdesk::roles::requester
