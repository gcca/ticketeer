#include "ticketeer/handling/role/supervisor/routes.hpp"

#include "ticketeer/handling/role/supervisor/routes/common.hpp"

namespace ticketeer {

using namespace ticketeer::handling::role::supervisor::routes::common;

void Supervisor::TicketAssignedToPost(const drogon::HttpRequestPtr &req,
                                      Callback &&callback,
                                      const std::string &ticket_id) {
  auto *logger = quill::Frontend::get_logger("root");
  const auto token = req->getCookie("token");
  const auto new_assigned_to = req->getParameter("new_assigned_to");

  sqlite3 *db = ConnectDB();
  if (!db)
    return BadRequest(callback, "Database unavailable",
                      drogon::k503ServiceUnavailable);

  const auto profile = FetchProfile(logger, db, token);
  if (!profile) {
    sqlite3_close(db);
    return BadRequest(callback, "Forbidden", drogon::k403Forbidden);
  }

  if (!UpdateTicketAssignedTo(logger, db, ticket_id, new_assigned_to)) {
    sqlite3_close(db);
    return BadRequest(callback, "Failed to update assigned to",
                      drogon::k400BadRequest);
  }

  auto assignees = FetchAssignees(logger, db);
  std::string assignee_name = "Sin asignar";
  for (const auto &a : assignees)
    if (a.at("id") == new_assigned_to) {
      assignee_name = a.at("name");
      break;
    }
  InsertTicketActivity(logger, db, ticket_id, profile->id, "assigned_changed",
                       assignee_name);
  sqlite3_close(db);

  drogon::HttpViewData data;
  data.insert("ticket_id", ticket_id);
  data.insert("assigned_to_id", new_assigned_to);
  data.insert("assigned_to_name", assignee_name);
  data.insert("assignees", assignees);
  callback(drogon::HttpResponse::newHttpViewResponse(
      "supervisor_ticket_assigned_to_select", data));
}

} // namespace ticketeer
