#include "routes.hpp"

#include <sqlite3.h>
#include <trantor/utils/Logger.h>

#include "ticketeer/core/db.hpp"
#include "ticketeer/handling/common.hpp"

namespace {

using ticketeer::core::db::ConnectRO;
using ticketeer::handling::common::Callback;

[[nodiscard]] inline std::string FetchRole(sqlite3 *db,
                                           const std::string &token) {
  sqlite3_stmt *stmt = nullptr;
  const char *sql = R"SQL(
SELECT p.role
  FROM auth_session s
  JOIN helpdesk_profile p
    ON p.user_id = s.user_id
 WHERE s.token = ?
   AND s.expires_at > datetime('now')
 LIMIT 1
)SQL";

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    LOG_DEBUG << "[dashboard] query error: " << sqlite3_errmsg(db);
    return {};
  }

  sqlite3_bind_text(stmt, 1, token.c_str(), -1, SQLITE_STATIC);

  std::string role;
  if (sqlite3_step(stmt) == SQLITE_ROW)
    role = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));

  sqlite3_finalize(stmt);
  return role;
}

} // namespace

namespace ticketeer {

void Dashboard::Index(const drogon::HttpRequestPtr &req, Callback &&callback) {
  const auto token = req->getCookie("token");

  if (token.empty())
    return callback(
        drogon::HttpResponse::newRedirectionResponse("/ticketeer/auth/signin"));

  sqlite3 *db = ConnectRO();
  if (!db)
    return callback(
        drogon::HttpResponse::newRedirectionResponse("/ticketeer/auth/signin"));

  const auto role = FetchRole(db, token);
  sqlite3_close(db);

  if (role.empty())
    return callback(
        drogon::HttpResponse::newRedirectionResponse("/ticketeer/auth/signin"));

  callback(drogon::HttpResponse::newRedirectionResponse("/ticketeer/" + role));
}

} // namespace ticketeer
