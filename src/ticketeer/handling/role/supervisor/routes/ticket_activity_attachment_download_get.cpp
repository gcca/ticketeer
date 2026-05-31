#include "ticketeer/handling/role/supervisor/routes.hpp"

#include "ticketeer/handling/role/supervisor/routes/common.hpp"

namespace ticketeer {

using namespace ticketeer::handling::role::supervisor::routes::common;

void Supervisor::TicketActivityAttachmentDownloadGet(
    const drogon::HttpRequestPtr &req, Callback &&callback,
    const std::string &ticket_id, const std::string &activity_id,
    const std::string &attachment_id) {
  auto *logger = quill::Frontend::get_logger("root");
  const auto token = req->getCookie("token");

  sqlite3 *db = ConnectDB();
  if (!db)
    return BadRequest(callback, "Database unavailable",
                      drogon::k503ServiceUnavailable);

  const auto profile = FetchProfile(logger, db, token);
  if (!profile) {
    sqlite3_close(db);
    return BadRequest(callback, "Forbidden", drogon::k403Forbidden);
  }

  auto file = FetchTicketActivityAttachmentFile(logger, db, ticket_id,
                                                activity_id, attachment_id);
  sqlite3_close(db);

  if (!file)
    return BadRequest(callback, "Attachment not found", drogon::k404NotFound);

  const auto abs_path =
      AttachmentAbsolutePath(file->file_path, file->file_name);
  if (!std::filesystem::is_regular_file(abs_path))
    return BadRequest(callback, "Attachment file not found",
                      drogon::k404NotFound);

  callback(drogon::HttpResponse::newFileResponse(
      abs_path.string(), file->file_name, drogon::CT_CUSTOM, file->mime_type,
      req));
}

} // namespace ticketeer
