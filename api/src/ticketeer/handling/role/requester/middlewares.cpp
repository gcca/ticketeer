#include "middlewares.hpp"

#include <string>

#include <json/json.h>
#include <libpq-fe.h>
#include <quill/Frontend.h>
#include <quill/LogMacros.h>

#include "ticketeer/core/conf.hpp"

namespace ticketeer::api::role::requester::middlewares {

void RoleRequesterRequired::invoke(const drogon::HttpRequestPtr &req,
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

  const char *params[] = {token.c_str()};
  PGresult *res = PQexecParams(pg,
                               R"SQL(
SELECT p.id::text
  FROM helpdesk.profile p
  JOIN auth.session s
    ON s.user_id = p.user_id
 WHERE s.token = $1
   AND s.expires_at > now()
   AND p.role = 'requester'::helpdesk.role
)SQL",
                               1, nullptr, params, nullptr, nullptr, 0);

  const auto status = PQresultStatus(res);
  const bool allowed = status == PGRES_TUPLES_OK && PQntuples(res) == 1;
  if (status != PGRES_TUPLES_OK)
    LOGJ_DEBUG(logger, "[requester] role middleware query error",
               PQresultErrorMessage(res));

  PQclear(res);
  PQfinish(pg);

  if (!allowed)
    return Reject(mcb, "Forbidden: insufficient permissions",
                  drogon::k403Forbidden);

  nextCb([mcb = std::move(mcb)](const drogon::HttpResponsePtr &resp) {
    mcb(resp);
  });
}

void RoleRequesterRequired::Reject(const drogon::MiddlewareCallback &mcb,
                                   const char *msg,
                                   drogon::HttpStatusCode code) {
  Json::Value error;
  error["message"] = msg;
  auto resp = drogon::HttpResponse::newHttpJsonResponse(error);
  resp->setStatusCode(code);
  mcb(resp);
}

} // namespace ticketeer::api::role::requester::middlewares
