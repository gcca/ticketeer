#include "ticketeer/handling/role/supervisor/routes.hpp"

#include "ticketeer/handling/role/supervisor/routes/common.hpp"

namespace ticketeer {

using namespace ticketeer::handling::role::supervisor::routes::common;

void Supervisor::ConfigTicketPriorityDeletePost(
    const drogon::HttpRequestPtr &req, Callback &&callback,
    const std::string &ticket_priority_id) {
  auto *logger = quill::Frontend::get_logger("root");
  const auto token = req->getCookie("token");

  sqlite3 *db = ConnectDB();
  if (!db)
    return BadRequest(callback, "Database unavailable",
                      drogon::k503ServiceUnavailable);

  const auto profile = FetchProfile(logger, db, token);
  if (!profile) {
    sqlite3_close(db);
    return BadRequest(callback, "Invalid or expired token",
                      drogon::k401Unauthorized);
  }

  if (!TicketPriorityExists(logger, db, ticket_priority_id)) {
    sqlite3_close(db);
    return BadRequest(callback, "Ticket priority not found",
                      drogon::k404NotFound);
  }

  const auto replacement_id = Trim(req->getParameter("replacement_id"));
  if (!replacement_id.empty()) {
    if (!TicketPriorityExists(logger, db, replacement_id)) {
      sqlite3_close(db);
      return BadRequest(callback, "Replacement priority not found",
                        drogon::k404NotFound);
    }
    if (!ReassignTicketPriority(logger, db, ticket_priority_id, replacement_id)) {
      sqlite3_close(db);
      return BadRequest(callback, "Failed to reassign ticket priority",
                        drogon::k400BadRequest);
    }
  }

  if (!DeleteTicketPriority(logger, db, ticket_priority_id)) {
    sqlite3_close(db);
    return BadRequest(callback, "Failed to remove ticket priority",
                      drogon::k400BadRequest);
  }

  const bool rendered = RenderTicketPriorityConfig(callback, logger, db, "");
  sqlite3_close(db);
  if (!rendered)
    return BadRequest(callback, "Failed to render ticket priorities");
}

} // namespace ticketeer
