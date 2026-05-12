#include "ticketeer/handling/role/supervisor/routes.hpp"
#include "ticketeer/handling/role/supervisor/routes/common.hpp"

namespace ticketeer {

using namespace ticketeer::handling::role::supervisor::routes::common;

void Supervisor::TicketPriorityPost(const drogon::HttpRequestPtr &req,
                                    Callback &&callback,
                                    const std::string &ticket_id) {
  const auto token = req->getCookie("token");
  const auto new_priority = req->getParameter("new_priority");
  if (new_priority.empty())
    return BadRequest(callback, "Missing field: new_priority");

  sqlite3 *db = ConnectDB();
  if (!db)
    return BadRequest(callback, "Database unavailable",
                      drogon::k503ServiceUnavailable);

  const auto profile = FetchProfile(db, token);
  if (!profile) {
    sqlite3_close(db);
    return BadRequest(callback, "Forbidden", drogon::k403Forbidden);
  }

  if (!UpdateTicketPriority(db, ticket_id, new_priority)) {
    sqlite3_close(db);
    return BadRequest(callback, "Failed to update priority",
                      drogon::k400BadRequest);
  }

  const auto priority_name = FetchPriorityName(db, new_priority);
  if (!priority_name) {
    sqlite3_close(db);
    return BadRequest(callback, "Priority not found", drogon::k404NotFound);
  }
  InsertTicketActivity(db, ticket_id, profile->id, "priority_changed",
                       *priority_name);
  sqlite3_close(db);

  drogon::HttpViewData data;
  data.insert("priority_name", *priority_name);
  callback(drogon::HttpResponse::newHttpViewResponse(
      "supervisor_ticket_priority_badge", data));
}

} // namespace ticketeer
