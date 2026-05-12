#include "ticketeer/handling/role/technician/routes.hpp"
#include "ticketeer/handling/role/technician/routes/common.hpp"

namespace ticketeer {

using namespace ticketeer::handling::role::technician::routes::common;

void Technician::TicketDetailsGet(const drogon::HttpRequestPtr &req,
                                  Callback &&callback,
                                  const std::string &ticket_id) {
  const auto token = req->getCookie("token");

  sqlite3 *db = ConnectRO();
  if (!db)
    return BadRequest(callback, "Database unavailable",
                      drogon::k503ServiceUnavailable);

  const auto profile = FetchProfile(db, token);
  if (!profile) {
    sqlite3_close(db);
    return BadRequest(callback, "Forbidden", drogon::k403Forbidden);
  }

  const auto ticket = FetchTicketDetails(db, ticket_id, profile->id);
  if (!ticket) {
    sqlite3_close(db);
    return BadRequest(callback, "Ticket not found", drogon::k404NotFound);
  }

  auto activities = FetchActivities(db, ticket_id);
  auto attachments = FetchAttachments(db, ticket_id);
  const auto activity_body_maxlength = FetchTicketActivityBodyMaxlength(db);
  sqlite3_close(db);

  drogon::HttpViewData data;
  data.insert("ticket", *ticket);
  data.insert("activities", activities);
  data.insert("attachments", attachments);
  data.insert("activity_body_maxlength",
              std::to_string(activity_body_maxlength));
  callback(drogon::HttpResponse::newHttpViewResponse(
      "technician_ticket_details", data));
}

} // namespace ticketeer
