#include "ticketeer/handling/role/supervisor/routes.hpp"

#include "ticketeer/handling/role/supervisor/routes/common.hpp"

namespace ticketeer {

using namespace ticketeer::handling::role::supervisor::routes::common;

void Supervisor::TicketActivityCreateMessagePost(
    const drogon::HttpRequestPtr &req, Callback &&callback,
    const std::string &ticket_id) {
  auto *logger = quill::Frontend::get_logger("root");
  const auto token = req->getCookie("token");

  std::string message = req->getParameter("message");
  if (message.empty()) {
    if (const auto body = req->getJsonObject()) {
      const auto &json_message = (*body)["message"];
      if (json_message.isString())
        message = json_message.asString();
    }
  }
  if (message.empty())
    return BadRequest(callback, "Missing field: message");

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

  const auto ticket_trait = FetchTicketTrait(logger, db, ticket_id);
  if (!ticket_trait) {
    sqlite3_close(db);
    return BadRequest(callback, "Ticket not found", drogon::k404NotFound);
  }
  if (*ticket_trait == "closed") {
    sqlite3_close(db);
    return BadRequest(callback, "Ticket is closed", drogon::k403Forbidden);
  }

  const auto activity_body_maxlength =
      FetchTicketActivityBodyMaxlength(logger, db);
  if (static_cast<int>(message.size()) > activity_body_maxlength) {
    sqlite3_close(db);
    return BadRequest(callback, "Activity body exceeds maximum length");
  }

  sqlite3_stmt *stmt = nullptr;
  const char *sql = R"SQL(
INSERT INTO helpdesk_ticket_activity (ticket_id, profile_id, kind, body)
VALUES (?, ?, 'message', ?)
)SQL";

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    LOGJ_DEBUG(logger, "[supervisor] insert activity error",
               sqlite3_errmsg(db));
    sqlite3_close(db);
    return BadRequest(callback, "Failed to create activity");
  }
  sqlite3_bind_text(stmt, 1, ticket_id.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 2, profile->id.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 3, message.c_str(), -1, SQLITE_STATIC);

  if (sqlite3_step(stmt) != SQLITE_DONE) {
    LOGJ_DEBUG(logger, "[supervisor] insert activity step error",
               sqlite3_errmsg(db));
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return BadRequest(callback, "Failed to create activity");
  }
  sqlite3_finalize(stmt);

  const auto last_id = std::to_string(sqlite3_last_insert_rowid(db));
  sqlite3_close(db);

  Row activity{{"id", last_id},
               {"kind", "message"},
               {"body", message},
               {"created_at", "ahora"},
               {"profile_name", profile->name}};

  drogon::HttpViewData data;
  data.insert("ticket_id", ticket_id);
  data.insert("activity", activity);
  callback(drogon::HttpResponse::newHttpViewResponse(
      "supervisor_ticket_activity_message", data));
}

} // namespace ticketeer
