#include "middlewares.hpp"

#include <string>

#include <json/json.h>
#include <sqlite3.h>
#include <trantor/utils/Logger.h>

#include "ticketeer/core/db.hpp"
#include "ticketeer/handling/auth/utils.hpp"

namespace ticketeer::handling::auth::middlewares {

void LogInRequired::invoke(const drogon::HttpRequestPtr &req,
                           drogon::MiddlewareNextCallback &&nextCb,
                           drogon::MiddlewareCallback &&mcb) {
  const auto token = req->getCookie("token");
  if (token.empty())
    return mcb(
        drogon::HttpResponse::newRedirectionResponse("/ticketeer/auth/signin"));

  sqlite3 *db = ticketeer::core::db::ConnectRO();
  if (!db)
    return Reject(mcb, "Database unavailable", drogon::k503ServiceUnavailable);

  const auto username = auth::utils::FetchUsername(db, token);
  sqlite3_close(db);

  if (!username) {
    LOG_DEBUG << "[auth/middleware] invalid or expired token, deleting session";
    sqlite3 *rw = ticketeer::core::db::Connect();
    if (rw) {
      const bool signed_out = auth::utils::SignOut(rw, token);
      if (!signed_out)
        LOG_DEBUG << "[auth/middleware] session already deleted or not found";
      sqlite3_close(rw);
    }
    drogon::Cookie cookie("token", "");
    cookie.setHttpOnly(true);
    cookie.setPath("/ticketeer");
    cookie.setExpiresDate(trantor::Date());
    auto resp =
        drogon::HttpResponse::newRedirectionResponse("/ticketeer/auth/signin");
    resp->addCookie(cookie);
    return mcb(resp);
  }

  nextCb(std::move(mcb));
}

void LogInRequired::Reject(const drogon::MiddlewareCallback &mcb,
                           const char *msg, drogon::HttpStatusCode code) {
  Json::Value error;
  error["message"] = msg;
  auto resp = drogon::HttpResponse::newHttpJsonResponse(error);
  resp->setStatusCode(code);
  mcb(resp);
}

} // namespace ticketeer::handling::auth::middlewares
