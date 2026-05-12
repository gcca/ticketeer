#include "ticketeer/handling/role/requester/routes.hpp"
#include "ticketeer/handling/role/requester/routes/common.hpp"

namespace ticketeer {

using namespace ticketeer::handling::role::requester::routes::common;

void Requester::TicketListGet(const drogon::HttpRequestPtr &req,
                              Callback &&callback) {
  const auto token = req->getCookie("token");

  sqlite3 *db = ConnectRO();
  if (!db)
    return BadRequest(callback, "Database unavailable",
                      drogon::k503ServiceUnavailable);

  const auto profile = FetchProfile(db, token);
  if (!profile) {
    sqlite3_close(db);
    return callback(
        drogon::HttpResponse::newRedirectionResponse("/ticketeer/auth/signin"));
  }

  TicketFilters filters{.search = req->getParameter("s"),
                        .status_id = req->getParameter("status_id"),
                        .priority_id = req->getParameter("priority_id"),
                        .sort = req->getParameter("sort"),
                        .sort_dir = req->getParameter("dir")};
  const auto page = ParsePage(req->getParameter("p"));
  auto data = BuildTicketListData(db, profile->id, filters, page);
  sqlite3_close(db);

  callback(
      drogon::HttpResponse::newHttpViewResponse("requester_ticket_list", data));
}

} // namespace ticketeer
