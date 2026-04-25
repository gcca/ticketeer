#pragma once

#include <drogon/HttpController.h>

namespace ticketeer::api {

class Dashboard : public drogon::HttpController<Dashboard> {
public:
  METHOD_LIST_BEGIN
  METHOD_ADD(Dashboard::Landing, "/v1/landing", drogon::Get);
  METHOD_LIST_END

  void Landing(const drogon::HttpRequestPtr &req,
               std::function<void(const drogon::HttpResponsePtr &)> &&callback);
};

} // namespace ticketeer::api
