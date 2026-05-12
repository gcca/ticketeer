#include "routes.hpp"

#include <quill/Frontend.h>
#include <quill/LogMacros.h>
#include <sqlite3.h>

#include "ticketeer/core/conf.hpp"

namespace {

using Callback = std::function<void(const drogon::HttpResponsePtr &)>;

[[nodiscard]] inline sqlite3 *ConnectDB() {
  std::string path = ticketeer::core::conf::settings.DB_URL;
  if (path.starts_with("sqlite:"))
    path = path.substr(7);
  sqlite3 *db = nullptr;
  if (sqlite3_open(path.c_str(), &db) != SQLITE_OK) {
    sqlite3_close(db);
    return nullptr;
  }
  return db;
}

[[nodiscard]] inline std::string FetchRole(quill::Logger *logger, sqlite3 *db,
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
    LOGJ_DEBUG(logger, "[dashboard] query error", sqlite3_errmsg(db));
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
  auto *logger = quill::Frontend::get_logger("root");
  const auto token = req->getCookie("token");

  if (token.empty())
    return callback(
        drogon::HttpResponse::newRedirectionResponse("/ticketeer/auth/signin"));

  sqlite3 *db = ConnectDB();
  if (!db)
    return callback(
        drogon::HttpResponse::newRedirectionResponse("/ticketeer/auth/signin"));

  const auto role = FetchRole(logger, db, token);
  sqlite3_close(db);

  if (role.empty())
    return callback(
        drogon::HttpResponse::newRedirectionResponse("/ticketeer/auth/signin"));

  callback(drogon::HttpResponse::newRedirectionResponse("/ticketeer/" + role));
}

} // namespace ticketeer
