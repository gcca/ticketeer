#include "ticketeer/handling/role/supervisor/routes.hpp"

#include "ticketeer/handling/role/supervisor/routes/common.hpp"

namespace ticketeer {

using namespace ticketeer::handling::role::supervisor::routes::common;

void Supervisor::TicketStatusPost(const drogon::HttpRequestPtr &req,
                                  Callback &&callback,
                                  const std::string &ticket_id) {
  auto *logger = quill::Frontend::get_logger("root");
  const auto token = req->getCookie("token");
  const auto new_status = req->getParameter("new_status");
  if (new_status.empty())
    return BadRequest(callback, "Missing field: new_status");

  sqlite3 *db = ConnectDB();
  if (!db)
    return BadRequest(callback, "Database unavailable",
                      drogon::k503ServiceUnavailable);

  const auto profile = FetchProfile(logger, db, token);
  if (!profile) {
    sqlite3_close(db);
    return BadRequest(callback, "Forbidden", drogon::k403Forbidden);
  }

  if (!UpdateTicketStatus(logger, db, ticket_id, new_status)) {
    sqlite3_close(db);
    return BadRequest(callback, "Failed to update status",
                      drogon::k400BadRequest);
  }

  const auto status_name = FetchStatusName(logger, db, new_status);
  if (!status_name) {
    sqlite3_close(db);
    return BadRequest(callback, "Status not found", drogon::k404NotFound);
  }
  const auto status_trait = FetchStatusTrait(logger, db, new_status);
  InsertTicketActivity(logger, db, ticket_id, profile->id, "status_changed",
                       *status_name);
  sqlite3_close(db);

  drogon::HttpViewData data;
  data.insert("status_name", *status_name);
  data.insert("status_trait", status_trait.value_or(""));
  callback(drogon::HttpResponse::newHttpViewResponse(
      "supervisor_ticket_status_badge", data));
}

} // namespace ticketeer
