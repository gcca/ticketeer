#include "ticketeer/handling/role/supervisor/routes.hpp"

#include "ticketeer/handling/role/supervisor/routes/common.hpp"

namespace ticketeer {

using namespace ticketeer::handling::role::supervisor::routes::common;

void Supervisor::TicketDetailsGet(const drogon::HttpRequestPtr &req,
                                  Callback &&callback,
                                  const std::string &ticket_id) {
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

  const auto ticket = FetchTicketDetails(logger, db, ticket_id);
  if (!ticket) {
    sqlite3_close(db);
    return BadRequest(callback, "Ticket not found", drogon::k404NotFound);
  }

  auto activities = FetchActivities(logger, db, ticket_id);
  auto attachments = FetchAttachments(logger, db, ticket_id);
  auto statuses = FetchStatuses(logger, db);
  auto assignees = FetchAssignees(logger, db);
  auto priorities = FetchPriorities(logger, db);
  const auto activity_body_maxlength =
      FetchTicketActivityBodyMaxlength(logger, db);
  sqlite3_close(db);

  drogon::HttpViewData data;
  data.insert("ticket", *ticket);
  data.insert("activities", activities);
  data.insert("attachments", attachments);
  data.insert("statuses", statuses);
  data.insert("assignees", assignees);
  data.insert("priorities", priorities);
  data.insert("activity_body_maxlength",
              std::to_string(activity_body_maxlength));
  callback(drogon::HttpResponse::newHttpViewResponse(
      "supervisor_ticket_details", data));
}

} // namespace ticketeer
