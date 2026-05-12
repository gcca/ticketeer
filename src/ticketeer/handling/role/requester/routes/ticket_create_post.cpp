#include <trantor/utils/Logger.h>

#include "ticketeer/core/text.hpp"
#include "ticketeer/handling/role/requester/routes.hpp"
#include "ticketeer/handling/role/requester/routes/common.hpp"
#include "ticketeer/notifications/clickup.hpp"

namespace ticketeer {

using namespace ticketeer::handling::role::requester::routes::common;

void Requester::TicketCreatePost(const drogon::HttpRequestPtr &req,
                                 Callback &&callback) {
  const auto token = req->getCookie("token");

  const auto input = ParseTicketCreateInput(req);
  if (!input)
    return BadRequest(callback,
                      "Missing or invalid required fields: priority_id, "
                      "body");

  sqlite3 *db = Connect();
  if (!db)
    return BadRequest(callback, "Database unavailable",
                      drogon::k503ServiceUnavailable);

  const auto profile = FetchProfile(db, token);
  if (!profile) {
    sqlite3_close(db);
    return BadRequest(callback, "Invalid or expired token",
                      drogon::k401Unauthorized);
  }

  const auto body_maxlength = FetchTicketBodyMaxlength(db);
  if (static_cast<int>(ticketeer::core::text::Utf8Length(input->body)) >
      body_maxlength) {
    sqlite3_close(db);
    return BadRequest(callback, "Ticket body exceeds maximum length");
  }

  const auto default_status_id = FetchDefaultStatusId(db);
  if (!default_status_id) {
    sqlite3_close(db);
    return BadRequest(callback, "Failed to get default status");
  }

  const auto default_assigned_to_id = FetchDefaultAssignedToId(db);
  if (!default_assigned_to_id) {
    sqlite3_close(db);
    return BadRequest(callback, "Failed to get default assigned to");
  }

  const auto ticket_due_delta = FetchTicketDueDelta(db);

  sqlite3_stmt *stmt = nullptr;
  const char *sql = R"SQL(
INSERT INTO helpdesk_ticket
  (requester_id, priority_id, status_id, assigned_to_id, body, due_date)
VALUES (?, ?, ?, ?, ?, date('now', '-5 hours', '+' || ? || ' seconds'))
)SQL";

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    LOG_DEBUG << "[requester] insert ticket error: " << sqlite3_errmsg(db);
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
    LOG_DEBUG << "[requester] insert ticket step error: " << sqlite3_errmsg(db);
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return BadRequest(callback, "Failed to create ticket");
  }
  sqlite3_finalize(stmt);

  const auto ticket_id = std::to_string(sqlite3_last_insert_rowid(db));
  const auto created_ticket = FetchTicketDetails(db, ticket_id, profile->id);

  auto data = BuildTicketListData(db, profile->id, TicketFilters{}, 1);
  sqlite3_close(db);

  if (created_ticket)
    notifications::clickup::NotifyTicketCreated(*created_ticket, profile->name);

  callback(
      drogon::HttpResponse::newHttpViewResponse("requester_ticket_list", data));
}

} // namespace ticketeer
