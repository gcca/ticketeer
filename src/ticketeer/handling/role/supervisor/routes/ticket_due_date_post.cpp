#include "ticketeer/handling/role/supervisor/routes.hpp"

#include "ticketeer/handling/role/supervisor/routes/common.hpp"

namespace ticketeer {

using namespace ticketeer::handling::role::supervisor::routes::common;

void Supervisor::TicketDueDatePost(const drogon::HttpRequestPtr &req,
                                   Callback &&callback,
                                   const std::string &ticket_id) {
  auto *logger = quill::Frontend::get_logger("root");
  const auto token = req->getCookie("token");
  const auto new_due_date = req->getParameter("new_due_date");
  if (!IsValidIsoDate(new_due_date))
    return BadRequest(callback, "Invalid field: new_due_date");

  sqlite3 *db = ConnectDB();
  if (!db)
    return BadRequest(callback, "Database unavailable",
                      drogon::k503ServiceUnavailable);

  const auto profile = FetchProfile(logger, db, token);
  if (!profile) {
    sqlite3_close(db);
    return BadRequest(callback, "Forbidden", drogon::k403Forbidden);
  }

  if (!UpdateTicketDueDate(logger, db, ticket_id, new_due_date)) {
    sqlite3_close(db);
    return BadRequest(callback, "Failed to update due date",
                      drogon::k400BadRequest);
  }

  InsertTicketActivity(logger, db, ticket_id, profile->id, "due_date_changed",
                       new_due_date.empty() ? "Sin fecha límite"
                                            : new_due_date);
  sqlite3_close(db);

  drogon::HttpViewData data;
  data.insert("ticket_id", ticket_id);
  data.insert("due_date", new_due_date);
  callback(drogon::HttpResponse::newHttpViewResponse(
      "supervisor_ticket_due_date_input", data));
}

} // namespace ticketeer
