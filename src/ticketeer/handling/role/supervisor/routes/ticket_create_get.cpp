#include "ticketeer/handling/role/supervisor/routes.hpp"

#include "ticketeer/handling/role/supervisor/routes/common.hpp"

namespace ticketeer {

using namespace ticketeer::handling::role::supervisor::routes::common;

void Supervisor::TicketCreateGet(const drogon::HttpRequestPtr &req,
                                 Callback &&callback) {
  auto *logger = quill::Frontend::get_logger("root");
  const auto token = req->getCookie("token");

  sqlite3 *db = ConnectDB();
  if (!db)
    return BadRequest(callback, "Database unavailable",
                      drogon::k503ServiceUnavailable);

  const auto profile = FetchProfile(logger, db, token);
  if (!profile) {
    sqlite3_close(db);
    return callback(
        drogon::HttpResponse::newRedirectionResponse("/ticketeer/auth/signin"));
  }

  auto priorities = FetchPriorities(logger, db);
  const auto body_maxlength = FetchTicketBodyMaxlength(logger, db);
  const auto default_ticket_priority_id =
      FetchDefaultTicketPriorityId(logger, db).value_or("");
  sqlite3_close(db);

  drogon::HttpViewData data;
  data.insert("priorities", priorities);
  data.insert("body_maxlength", std::to_string(body_maxlength));
  data.insert("default_ticket_priority_id", default_ticket_priority_id);
  callback(drogon::HttpResponse::newHttpViewResponse("supervisor_ticket_create",
                                                     data));
}

} // namespace ticketeer
