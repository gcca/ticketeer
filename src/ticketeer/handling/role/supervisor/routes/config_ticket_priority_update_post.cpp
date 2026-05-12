#include "ticketeer/handling/role/supervisor/routes.hpp"
#include "ticketeer/handling/role/supervisor/routes/common.hpp"

namespace ticketeer {

using namespace ticketeer::handling::role::supervisor::routes::common;

void Supervisor::ConfigTicketPriorityUpdatePost(
    const drogon::HttpRequestPtr &req, Callback &&callback,
    const std::string &ticket_priority_id) {
  const auto token = req->getCookie("token");

  const auto input = ParseTicketPriorityInput(req);
  if (!input)
    return BadRequest(callback,
                      "Missing or invalid required fields: name, display_name");

  sqlite3 *db = ConnectDB();
  if (!db)
    return BadRequest(callback, "Database unavailable",
                      drogon::k503ServiceUnavailable);

  const auto profile = FetchProfile(db, token);
  if (!profile) {
    sqlite3_close(db);
    return BadRequest(callback, "Invalid or expired token",
                      drogon::k401Unauthorized);
  }

  if (!UpdateTicketPriorityRecord(db, ticket_priority_id, *input)) {
    sqlite3_close(db);
    return BadRequest(callback, "Failed to save ticket priority",
                      drogon::k400BadRequest);
  }

  const bool rendered = RenderTicketPriorityConfig(callback, db, "");
  sqlite3_close(db);
  if (!rendered)
    return BadRequest(callback, "Failed to render ticket priorities");
}

} // namespace ticketeer
