#include "ticketeer/handling/role/supervisor/routes.hpp"
#include "ticketeer/handling/role/supervisor/routes/common.hpp"

namespace ticketeer {

using namespace ticketeer::handling::role::supervisor::routes::common;

void Supervisor::ConfigTicketPriorityDeletePost(
    const drogon::HttpRequestPtr &req, Callback &&callback,
    const std::string &ticket_priority_id) {
  const auto token = req->getCookie("token");

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

  if (!TicketPriorityExists(db, ticket_priority_id)) {
    sqlite3_close(db);
    return BadRequest(callback, "Ticket priority not found",
                      drogon::k404NotFound);
  }

  const auto replacement_id = Trim(req->getParameter("replacement_id"));
  if (!replacement_id.empty()) {
    if (!TicketPriorityExists(db, replacement_id)) {
      sqlite3_close(db);
      return BadRequest(callback, "Replacement priority not found",
                        drogon::k404NotFound);
    }
    if (!ReassignTicketPriority(db, ticket_priority_id, replacement_id)) {
      sqlite3_close(db);
      return BadRequest(callback, "Failed to reassign ticket priority",
                        drogon::k400BadRequest);
    }
  }

  if (!DeleteTicketPriority(db, ticket_priority_id)) {
    sqlite3_close(db);
    return BadRequest(callback, "Failed to remove ticket priority",
                      drogon::k400BadRequest);
  }

  const bool rendered = RenderTicketPriorityConfig(callback, db, "");
  sqlite3_close(db);
  if (!rendered)
    return BadRequest(callback, "Failed to render ticket priorities");
}

} // namespace ticketeer
