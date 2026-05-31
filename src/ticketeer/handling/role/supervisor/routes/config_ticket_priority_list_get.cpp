#include "ticketeer/handling/role/supervisor/routes.hpp"

#include "ticketeer/handling/role/supervisor/routes/common.hpp"

namespace ticketeer {

using namespace ticketeer::handling::role::supervisor::routes::common;

void Supervisor::ConfigTicketPriorityListGet(const drogon::HttpRequestPtr &req,
                                             Callback &&callback) {
  auto *logger = quill::Frontend::get_logger("root");
  const auto token = req->getCookie("token");

  sqlite3 *db = ConnectDB();
  if (!db)
    return BadRequest(callback, "Database unavailable",
                      drogon::k503ServiceUnavailable);

  const auto profile = FetchProfile(logger, db, token);
  if (!profile) {
    sqlite3_close(db);
    return callback(
        drogon::HttpResponse::newRedirectionResponse("/ticketeer/auth/signin"));
  }

  const auto editing_id = req->getParameter("tp");
  const bool rendered =
      RenderTicketPriorityConfig(callback, logger, db, editing_id);
  sqlite3_close(db);
  if (!rendered)
    return BadRequest(callback, "Ticket priority not found",
                      drogon::k404NotFound);
}

} // namespace ticketeer
