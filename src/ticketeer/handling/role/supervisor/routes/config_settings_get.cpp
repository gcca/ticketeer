#include "ticketeer/handling/role/supervisor/routes.hpp"
#include "ticketeer/handling/role/supervisor/routes/common.hpp"

namespace ticketeer {

using namespace ticketeer::handling::role::supervisor::routes::common;

void Supervisor::ConfigSettingsGet(const drogon::HttpRequestPtr &req,
                                   Callback &&callback) {
  const auto token = req->getCookie("token");

  sqlite3 *db = ConnectRO();
  if (!db)
    return BadRequest(callback, "Database unavailable",
                      drogon::k503ServiceUnavailable);

  const auto profile = FetchProfile(db, token);
  if (!profile) {
    sqlite3_close(db);
    return callback(
        drogon::HttpResponse::newRedirectionResponse("/ticketeer/auth/signin"));
  }

  RenderSettingsConfig(callback, db);
  sqlite3_close(db);
}

} // namespace ticketeer
