#include "middlewares.hpp"

#include <string>

#include <json/json.h>
#include <quill/Frontend.h>
#include <sqlite3.h>

#include "ticketeer/core/conf.hpp"
#include "ticketeer/handling/auth/utils.hpp"

namespace ticketeer::handling::auth::middlewares {

void LogInRequired::invoke(const drogon::HttpRequestPtr &req,
                           drogon::MiddlewareNextCallback &&nextCb,
                           drogon::MiddlewareCallback &&mcb) {
  const auto token = req->getCookie("token");
  if (token.empty())
    return mcb(drogon::HttpResponse::newRedirectionResponse("/ticketeer/auth/signin"));
  auto *logger = quill::Frontend::get_logger("root");

  std::string db_path = ticketeer::core::conf::settings.DB_URL;
  if (db_path.starts_with("sqlite:")) db_path = db_path.substr(7);

  sqlite3 *db = nullptr;
  if (sqlite3_open(db_path.c_str(), &db) != SQLITE_OK) {
    sqlite3_close(db);
    return Reject(mcb, "Database unavailable", drogon::k503ServiceUnavailable);
  }

  const auto username = auth::utils::FetchUsername(logger, db, token);
  sqlite3_close(db);

  if (!username)
    return mcb(drogon::HttpResponse::newRedirectionResponse("/ticketeer/auth/signin"));

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
