#include "ticketeer/handling/role/supervisor/routes.hpp"

#include <drogon/MultiPart.h>

#include "ticketeer/handling/role/supervisor/routes/common.hpp"

namespace ticketeer {

using namespace ticketeer::handling::role::supervisor::routes::common;

void Supervisor::TicketActivityAttachmentCreatePost(
    const drogon::HttpRequestPtr &req, Callback &&callback,
    const std::string &ticket_id, const std::string &activity_id) {
  auto *logger = quill::Frontend::get_logger("root");
  const auto token = req->getCookie("token");

  if (req->contentType() != drogon::CT_MULTIPART_FORM_DATA)
    return BadRequest(callback, "Content-Type must be multipart/form-data");

  drogon::MultiPartParser parser;
  if (parser.parse(req) != 0)
    return BadRequest(callback, "Invalid multipart form-data");

  const auto &files = parser.getFiles();
  if (files.empty())
    return BadRequest(callback, "Missing file");

  std::size_t upload_total = 0;
  for (const auto &f : files)
    upload_total += f.fileLength();
  if (upload_total > MaxActivityAttachmentSize)
    return BadRequest(callback, "Files exceed 25 MB per activity limit",
                      drogon::k413RequestEntityTooLarge);

  sqlite3 *db = ConnectDB();
  if (!db)
    return BadRequest(callback, "Database unavailable",
                      drogon::k503ServiceUnavailable);

  const auto profile = FetchProfile(logger, db, token);
  if (!profile) {
    sqlite3_close(db);
    return BadRequest(callback, "Forbidden", drogon::k403Forbidden);
  }

  if (!TicketActivityExists(logger, db, ticket_id, activity_id)) {
    sqlite3_close(db);
    return BadRequest(callback, "Ticket activity not found",
                      drogon::k404NotFound);
  }

  if (FetchActivityAttachmentTotal(db, activity_id) + upload_total >
      MaxActivityAttachmentSize) {
    sqlite3_close(db);
    return BadRequest(callback, "Activity attachments would exceed 25 MB",
                      drogon::k413RequestEntityTooLarge);
  }

  if (FetchTicketAttachmentTotal(db, ticket_id) + upload_total >
      MaxTicketAttachmentSize) {
    sqlite3_close(db);
    return BadRequest(callback, "Ticket attachments would exceed 75 MB",
                      drogon::k413RequestEntityTooLarge);
  }

  std::string html;
  for (const auto &upload_file : files) {
    const std::string fname =
        std::filesystem::path(upload_file.getFileName()).filename().string();
    if (fname.empty())
      continue;
    const auto fsize = upload_file.fileLength();
    const std::string mime = InferMimeType(fname);

    if (!Exec(db, "BEGIN IMMEDIATE"))
      continue;

    sqlite3_stmt *insert = nullptr;
    if (sqlite3_prepare_v2(db,
                           "INSERT INTO helpdesk_ticket_activity_attachment"
                           " (ticket_activity_id, file_path, file_name,"
                           "  file_size, mime_type)"
                           " VALUES (?, '', ?, ?, ?)",
                           -1, &insert, nullptr) != SQLITE_OK) {
      Exec(db, "ROLLBACK");
      continue;
    }
    sqlite3_bind_text(insert, 1, activity_id.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(insert, 2, fname.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int64(insert, 3, static_cast<sqlite3_int64>(fsize));
    sqlite3_bind_text(insert, 4, mime.c_str(), -1, SQLITE_STATIC);
    if (sqlite3_step(insert) != SQLITE_DONE) {
      sqlite3_finalize(insert);
      Exec(db, "ROLLBACK");
      continue;
    }
    sqlite3_finalize(insert);

    const auto att_id = std::to_string(sqlite3_last_insert_rowid(db));
    const std::filesystem::path rel =
        std::filesystem::path("role") / ("ticket_id=" + ticket_id) /
        ("activity_id=" + activity_id) / ("attachment_id=" + att_id);
    const auto abs_path = std::filesystem::current_path() /
                          ticketeer::core::conf::settings.UPLOAD_DIR / rel /
                          fname;

    if (!WriteFile(abs_path, upload_file.fileContent())) {
      Exec(db, "ROLLBACK");
      continue;
    }

    sqlite3_stmt *upd = nullptr;
    const std::string rel_str = rel.generic_string();
    if (sqlite3_prepare_v2(
            db,
            "UPDATE helpdesk_ticket_activity_attachment SET file_path = ?"
            " WHERE id = ?",
            -1, &upd, nullptr) != SQLITE_OK ||
        [&] {
          sqlite3_bind_text(upd, 1, rel_str.c_str(), -1, SQLITE_STATIC);
          sqlite3_bind_text(upd, 2, att_id.c_str(), -1, SQLITE_STATIC);
          return sqlite3_step(upd) != SQLITE_DONE;
        }()) {
      if (upd)
        sqlite3_finalize(upd);
      std::error_code ec;
      std::filesystem::remove(abs_path, ec);
      Exec(db, "ROLLBACK");
      continue;
    }
    sqlite3_finalize(upd);

    if (!Exec(db, "COMMIT")) {
      std::error_code ec;
      std::filesystem::remove(abs_path, ec);
      continue;
    }

    html +=
        "<a class=\"badge badge-outline gap-1\" href=\"/ticketeer/supervisor"
        "/ticket/" +
        ticket_id + "/activity/" + activity_id + "/attachment/" + att_id +
        "/download\">" +
        std::string(drogon::HttpViewData::htmlTranslate(fname)) + "</a>\n";
  }
  sqlite3_close(db);

  auto resp = drogon::HttpResponse::newHttpResponse();
  resp->setContentTypeCode(drogon::CT_NONE);
  resp->addHeader("Content-Type", "text/html; charset=utf-8");
  resp->setBody(html);
  callback(resp);
}

} // namespace ticketeer
