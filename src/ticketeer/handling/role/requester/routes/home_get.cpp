#include "ticketeer/handling/role/requester/routes.hpp"
#include "ticketeer/handling/role/requester/routes/common.hpp"

namespace ticketeer {

using namespace ticketeer::handling::role::requester::routes::common;

void Requester::HomeGet(const drogon::HttpRequestPtr &req,
                        Callback &&callback) {
  const auto token = req->getCookie("token");

  sqlite3 *db = ConnectRO();
  if (!db)
    return BadRequest(callback, "Database unavailable",
                      drogon::k503ServiceUnavailable);

  const auto profile = FetchProfile(db, token);
  sqlite3_close(db);

  if (!profile)
    return callback(
        drogon::HttpResponse::newRedirectionResponse("/ticketeer/auth/signin"));

  drogon::HttpViewData data;
  data.insert("username", profile->username);
  data.insert("name", profile->name);
  callback(drogon::HttpResponse::newHttpViewResponse("requester_home", data));
}

} // namespace ticketeer
