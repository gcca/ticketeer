#include "ticketeer/handling/role/supervisor/routes.hpp"

#include "ticketeer/handling/role/supervisor/routes/common.hpp"

namespace ticketeer {

using namespace ticketeer::handling::role::supervisor::routes::common;

void Supervisor::TicketActivityListGet(const drogon::HttpRequestPtr &req,
                                       Callback &&callback,
                                       const std::string &ticket_id) {
  auto *logger = quill::Frontend::get_logger("root");
  const auto token = req->getCookie("token");

  sqlite3 *db = ConnectDB();
  if (!db)
    return BadRequest(callback, "Database unavailable",
                      drogon::k503ServiceUnavailable);

  const auto profile = FetchProfile(logger, db, token);
  if (!profile) {
    sqlite3_close(db);
    return BadRequest(callback, "Forbidden", drogon::k403Forbidden);
  }

  if (!TicketExists(logger, db, ticket_id)) {
    sqlite3_close(db);
    return BadRequest(callback, "Ticket not found", drogon::k404NotFound);
  }

  auto activities = FetchActivities(logger, db, ticket_id);
  auto attachments = FetchAttachments(logger, db, ticket_id);
  sqlite3_close(db);

  drogon::HttpViewData data;
  data.insert("ticket_id", ticket_id);
  data.insert("activities", activities);
  data.insert("attachments", attachments);
  callback(drogon::HttpResponse::newHttpViewResponse(
      "supervisor_ticket_activity_list", data));
}

} // namespace ticketeer
