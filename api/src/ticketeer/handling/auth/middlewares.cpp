#include "middlewares.hpp"

#include <string>

#include <json/json.h>
#include <libpq-fe.h>
#include <quill/Frontend.h>

#include "ticketeer/core/conf.hpp"
#include "ticketeer/handling/auth/utils.hpp"

namespace ticketeer::api::auth::middlewares {

void LogInRequired::invoke(const drogon::HttpRequestPtr &req,
                           drogon::MiddlewareNextCallback &&nextCb,
                           drogon::MiddlewareCallback &&mcb) {
  const auto auth_header = req->getHeader("Authorization");
  if (auth_header.empty() || auth_header.rfind("Bearer ", 0) != 0)
    return Reject(mcb, "Missing Authorization", drogon::k400BadRequest);

  const std::string token = auth_header.substr(7);
  auto *logger = quill::Frontend::get_logger("root");
  PGconn *pg = PQconnectdb(ticketeer::core::conf::settings.DB_URL.c_str());
  if (PQstatus(pg) != CONNECTION_OK) {
    PQfinish(pg);
    return Reject(mcb, "Database unavailable", drogon::k503ServiceUnavailable);
  }

  const auto username = auth::utils::FetchUsername(logger, pg, token);
  PQfinish(pg);

  if (!username)
    return Reject(mcb, "Invalid or expired token", drogon::k401Unauthorized);

  nextCb([mcb = std::move(mcb)](const drogon::HttpResponsePtr &resp) {
    mcb(resp);
  });
}

void LogInRequired::Reject(const drogon::MiddlewareCallback &mcb,
                           const char *msg, drogon::HttpStatusCode code) {
  Json::Value error;
  error["message"] = msg;
  auto resp = drogon::HttpResponse::newHttpJsonResponse(error);
  resp->setStatusCode(code);
  mcb(resp);
}

} // namespace ticketeer::api::auth::middlewares
