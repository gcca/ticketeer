#include "routes.hpp"

#include "ticketeer/handling/common.hpp"

namespace ticketeer {

using ticketeer::handling::common::Callback;

void Base::Index(const drogon::HttpRequestPtr &, Callback &&callback) {
  callback(
      drogon::HttpResponse::newRedirectionResponse("/ticketeer/auth/signin"));
}

void Base::Healthcheck(const drogon::HttpRequestPtr &, Callback &&callback) {
  auto response = drogon::HttpResponse::newHttpResponse();
  response->setBody("🍺");
  callback(response);
}

} // namespace ticketeer
