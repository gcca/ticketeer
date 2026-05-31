#include "ticketeer/handling/role/supervisor/routes.hpp"

#include "ticketeer/handling/role/supervisor/routes/common.hpp"

namespace ticketeer {

using namespace ticketeer::handling::role::supervisor::routes::common;

void Supervisor::TicketListGet(const drogon::HttpRequestPtr &req,
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

  TicketFilters filters{.search = req->getParameter("s"),
                        .requester_id = req->getParameter("requester_id"),
                        .status_id = req->getParameter("status_id"),
                        .priority_id = req->getParameter("priority_id"),
                        .assigned_to_id = req->getParameter("assigned_to_id"),
                        .sort = req->getParameter("sort"),
                        .sort_dir = req->getParameter("dir")};
  const auto page = ParsePage(req->getParameter("p"));
  auto data = BuildTicketListData(logger, db, filters, page);
  sqlite3_close(db);

  callback(drogon::HttpResponse::newHttpViewResponse("supervisor_ticket_list",
                                                     data));
}

} // namespace ticketeer
