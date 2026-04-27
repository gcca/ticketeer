#pragma once

#include <drogon/HttpController.h>

#include "ticketeer/handling/auth/middlewares.hpp"

namespace ticketeer::api {

class Dashboard : public drogon::HttpController<Dashboard> {
public:
  METHOD_LIST_BEGIN
  METHOD_ADD(Dashboard::Landing, "/v1/landing", drogon::Get,
             "ticketeer::api::auth::middlewares::LogInRequired");
  METHOD_LIST_END

  void Landing(const drogon::HttpRequestPtr &req,
               std::function<void(const drogon::HttpResponsePtr &)> &&callback);
};

} // namespace ticketeer::api
