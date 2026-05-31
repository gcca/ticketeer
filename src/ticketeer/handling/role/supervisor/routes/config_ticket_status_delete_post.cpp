#include "ticketeer/handling/role/supervisor/routes.hpp"

#include "ticketeer/handling/role/supervisor/routes/common.hpp"

namespace ticketeer {

using namespace ticketeer::handling::role::supervisor::routes::common;

void Supervisor::ConfigTicketStatusDeletePost(
    const drogon::HttpRequestPtr &req, Callback &&callback,
    const std::string &ticket_status_id) {
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

  if (!TicketStatusExists(logger, db, ticket_status_id)) {
    sqlite3_close(db);
    return BadRequest(callback, "Ticket status not found",
                      drogon::k404NotFound);
  }

  const auto replacement_id = Trim(req->getParameter("replacement_id"));
  if (!replacement_id.empty()) {
    if (!TicketStatusExists(logger, db, replacement_id)) {
      sqlite3_close(db);
      return BadRequest(callback, "Replacement status not found",
                        drogon::k404NotFound);
    }
    if (!ReassignTicketStatus(logger, db, ticket_status_id, replacement_id)) {
      sqlite3_close(db);
      return BadRequest(callback, "Failed to reassign ticket status",
                        drogon::k400BadRequest);
    }
  }

  if (!DeleteTicketStatus(logger, db, ticket_status_id)) {
    sqlite3_close(db);
    return BadRequest(callback, "Failed to remove ticket status",
                      drogon::k400BadRequest);
  }

  const bool rendered = RenderTicketStatusConfig(callback, logger, db, "");
  sqlite3_close(db);
  if (!rendered)
    return BadRequest(callback, "Failed to render ticket statuses");
}

} // namespace ticketeer
