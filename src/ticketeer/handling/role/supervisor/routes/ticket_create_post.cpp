#include "ticketeer/handling/role/supervisor/routes.hpp"

#include "ticketeer/handling/role/supervisor/routes/common.hpp"

namespace ticketeer {

using namespace ticketeer::handling::role::supervisor::routes::common;

void Supervisor::TicketCreatePost(const drogon::HttpRequestPtr &req,
                                  Callback &&callback) {
  auto *logger = quill::Frontend::get_logger("root");
  const auto token = req->getCookie("token");

  const auto input = ParseTicketCreateInput(req);
  if (!input)
    return BadRequest(callback, "Missing or invalid required fields: "
                                "priority_id, body");

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

  const auto body_maxlength = FetchTicketBodyMaxlength(logger, db);
  if (static_cast<int>(input->body.size()) > body_maxlength) {
    sqlite3_close(db);
    return BadRequest(callback, "Ticket body exceeds maximum length");
  }

  const auto default_status_id = FetchDefaultStatusId(logger, db);
  if (!default_status_id) {
    sqlite3_close(db);
    return BadRequest(callback, "Failed to get default status");
  }

  const auto default_assigned_to_id = FetchDefaultAssignedToId(logger, db);
  if (!default_assigned_to_id) {
    sqlite3_close(db);
    return BadRequest(callback, "Failed to get default assigned to");
  }

  const auto ticket_due_delta = FetchTicketDueDelta(logger, db);

  sqlite3_stmt *stmt = nullptr;
  const char *sql = R"SQL(
INSERT INTO helpdesk_ticket
  (requester_id, priority_id, status_id, assigned_to_id, body, due_date)
VALUES (?, ?, ?, ?, ?, date('now', '-5 hours', '+' || ? || ' seconds'))
)SQL";

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    LOGJ_DEBUG(logger, "[supervisor] insert ticket error", sqlite3_errmsg(db));
    sqlite3_close(db);
    return BadRequest(callback, "Failed to create ticket");
  }

  sqlite3_bind_text(stmt, 1, profile->id.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 2, input->priority_id.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 3, default_status_id->c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 4, default_assigned_to_id->c_str(), -1,
                    SQLITE_STATIC);
  sqlite3_bind_text(stmt, 5, input->body.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_int(stmt, 6, ticket_due_delta);

  if (sqlite3_step(stmt) != SQLITE_DONE) {
    LOGJ_DEBUG(logger, "[supervisor] insert ticket step error",
               sqlite3_errmsg(db));
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return BadRequest(callback, "Failed to create ticket");
  }
  sqlite3_finalize(stmt);

  auto data = BuildTicketListData(logger, db, TicketFilters{}, 1);
  sqlite3_close(db);

  callback(drogon::HttpResponse::newHttpViewResponse("supervisor_ticket_list",
                                                     data));
}

} // namespace ticketeer
