#include <trantor/utils/Logger.h>

#include "ticketeer/core/text.hpp"
#include "ticketeer/handling/role/requester/routes.hpp"
#include "ticketeer/handling/role/requester/routes/common.hpp"

namespace ticketeer {

using namespace ticketeer::handling::role::requester::routes::common;

void Requester::TicketActivityCreateMessagePost(
    const drogon::HttpRequestPtr &req, Callback &&callback,
    const std::string &ticket_id) {
  const auto token = req->getCookie("token");

  const auto message = req->getParameter("message");
  if (message.empty())
    return BadRequest(callback, "Missing field: message");

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

  const auto ticket_trait =
      FetchTicketTraitForRequester(db, ticket_id, profile->id);
  if (!ticket_trait) {
    sqlite3_close(db);
    return BadRequest(callback, "Ticket not found", drogon::k404NotFound);
  }
  if (*ticket_trait == "closed") {
    sqlite3_close(db);
    return BadRequest(callback, "Ticket is closed", drogon::k403Forbidden);
  }

  const auto activity_body_maxlength = FetchTicketActivityBodyMaxlength(db);
  if (static_cast<int>(ticketeer::core::text::Utf8Length(message)) >
      activity_body_maxlength) {
    sqlite3_close(db);
    return BadRequest(callback, "Activity body exceeds maximum length");
  }

  sqlite3_stmt *stmt = nullptr;
  const char *sql = R"SQL(
INSERT INTO helpdesk_ticket_activity (ticket_id, profile_id, kind, body)
SELECT ?, p.id, 'message', ?
  FROM helpdesk_ticket t
  JOIN helpdesk_profile p ON p.id = ?
 WHERE t.id = ?
   AND t.requester_id = p.id
)SQL";

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    LOG_DEBUG << "[requester] insert activity error: " << sqlite3_errmsg(db);
    sqlite3_close(db);
    return BadRequest(callback, "Failed to create activity");
  }
  sqlite3_bind_text(stmt, 1, ticket_id.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 2, message.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 3, profile->id.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 4, ticket_id.c_str(), -1, SQLITE_STATIC);

  if (sqlite3_step(stmt) != SQLITE_DONE || sqlite3_changes(db) != 1) {
    LOG_DEBUG << "[requester] insert activity step error: "
              << sqlite3_errmsg(db);
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return BadRequest(callback, "Failed to create activity");
  }
  sqlite3_finalize(stmt);

  const auto last_id = std::to_string(sqlite3_last_insert_rowid(db));
  sqlite3_close(db);

  Row activity{{"id", last_id},
               {"body", message},
               {"created_at", "ahora"},
               {"profile_name", profile->name}};

  drogon::HttpViewData data;
  data.insert("ticket_id", ticket_id);
  data.insert("activity", activity);
  callback(drogon::HttpResponse::newHttpViewResponse(
      "requester_ticket_activity_message", data));
}

} // namespace ticketeer
