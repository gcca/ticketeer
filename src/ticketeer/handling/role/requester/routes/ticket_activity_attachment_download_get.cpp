#include <trantor/utils/Logger.h>

#include "ticketeer/handling/role/requester/routes.hpp"
#include "ticketeer/handling/role/requester/routes/common.hpp"

namespace ticketeer {

using namespace ticketeer::handling::role::requester::routes::common;

void Requester::TicketActivityAttachmentDownloadGet(
    const drogon::HttpRequestPtr &req, Callback &&callback,
    const std::string &ticket_id, const std::string &activity_id,
    const std::string &attachment_id) {
  const auto token = req->getCookie("token");

  sqlite3 *db = ConnectRO();
  if (!db)
    return BadRequest(callback, "Database unavailable",
                      drogon::k503ServiceUnavailable);

  sqlite3_stmt *stmt = nullptr;
  const char *sql = R"SQL(
SELECT taa.file_path, taa.file_name, taa.mime_type
  FROM helpdesk_ticket_activity_attachment taa
  JOIN helpdesk_ticket_activity ta ON ta.id = taa.ticket_activity_id
  JOIN helpdesk_ticket t            ON t.id  = ta.ticket_id
  JOIN helpdesk_profile p           ON p.id  = t.requester_id
  JOIN auth_session s               ON s.user_id = p.user_id
 WHERE taa.id = ?
   AND ta.id  = ?
   AND t.id   = ?
   AND s.token = ?
   AND s.expires_at > datetime('now')
   AND p.role = 'requester'
 LIMIT 1
)SQL";

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    LOG_DEBUG << "[requester] attachment download query error: "
              << sqlite3_errmsg(db);
    sqlite3_close(db);
    return BadRequest(callback, "Attachment not found", drogon::k404NotFound);
  }
  sqlite3_bind_text(stmt, 1, attachment_id.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 2, activity_id.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 3, ticket_id.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 4, token.c_str(), -1, SQLITE_STATIC);

  std::string file_path, file_name, mime_type;
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    file_path = ColumnText(stmt, 0);
    file_name = ColumnText(stmt, 1);
    mime_type = ColumnText(stmt, 2);
  }
  sqlite3_finalize(stmt);
  sqlite3_close(db);

  if (file_name.empty())
    return BadRequest(callback, "Attachment not found", drogon::k404NotFound);

  const auto abs_path = AttachmentAbsolutePath(file_path, file_name);
  if (!std::filesystem::is_regular_file(abs_path))
    return BadRequest(callback, "Attachment file not found",
                      drogon::k404NotFound);

  callback(drogon::HttpResponse::newFileResponse(
      abs_path.string(), file_name, drogon::CT_CUSTOM, mime_type, req));
}

} // namespace ticketeer
