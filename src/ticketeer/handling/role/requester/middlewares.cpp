#include "middlewares.hpp"

#include <sqlite3.h>
#include <quill/Frontend.h>
#include <quill/LogMacros.h>

#include "ticketeer/core/conf.hpp"

namespace ticketeer::handling::role::requester::middlewares {

void RoleRequesterRequired::invoke(const drogon::HttpRequestPtr &req,
                                   drogon::MiddlewareNextCallback &&nextCb,
                                   drogon::MiddlewareCallback &&mcb) {
  const auto token = req->getCookie("token");
  if (token.empty())
    return mcb(drogon::HttpResponse::newRedirectionResponse("/ticketeer/auth/signin"));

  auto *logger = quill::Frontend::get_logger("root");

  std::string path = ticketeer::core::conf::settings.DB_URL;
  if (path.starts_with("sqlite:")) path = path.substr(7);

  sqlite3 *db = nullptr;
  if (sqlite3_open(path.c_str(), &db) != SQLITE_OK) {
    sqlite3_close(db);
    return Reject(mcb, "Database unavailable", drogon::k503ServiceUnavailable);
  }

  sqlite3_stmt *stmt = nullptr;
  const char *sql = R"SQL(
SELECT p.id
  FROM helpdesk_profile p
  JOIN auth_session s
    ON s.user_id = p.user_id
 WHERE s.token = ?
   AND s.expires_at > datetime('now')
   AND p.role = 'requester'
 LIMIT 1
)SQL";

  bool allowed = false;
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
    sqlite3_bind_text(stmt, 1, token.c_str(), -1, SQLITE_STATIC);
    allowed = sqlite3_step(stmt) == SQLITE_ROW;
    sqlite3_finalize(stmt);
  } else {
    LOGJ_DEBUG(logger, "[requester] role middleware query error",
               sqlite3_errmsg(db));
  }

  sqlite3_close(db);

  if (!allowed)
    return mcb(drogon::HttpResponse::newRedirectionResponse("/ticketeer/auth/signin"));

  nextCb([mcb = std::move(mcb)](const drogon::HttpResponsePtr &resp) {
    mcb(resp);
  });
}

void RoleRequesterRequired::Reject(const drogon::MiddlewareCallback &mcb,
                                   const char *msg,
                                   drogon::HttpStatusCode code) {
  auto resp = drogon::HttpResponse::newHttpResponse();
  resp->setStatusCode(code);
  resp->setBody(msg);
  mcb(resp);
}

} // namespace ticketeer::handling::role::requester::middlewares
