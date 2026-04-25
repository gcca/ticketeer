#pragma once

#include <crow_all.h>

#include "internal.hpp"
#include "ticketeer/db/pg.hpp"

namespace ticketeer::apps::auth {

inline void RegisterRoutes(crow::SimpleApp &app) {
  namespace cr = ticketeer::core;

  CROW_ROUTE(app, "/ticketeer/api/v1/auth/signin")
      .methods(crow::HTTPMethod::POST)(
          [](const crow::request &req) -> crow::response {
            ticketeer::db::Pg db;
            if (!db.connected())
              return crow::response(503, db.conn_error());
            ticketeer::core::Context ctx;
            return SignIn(req, db, ctx);
          });
}

} // namespace ticketeer::apps::auth
