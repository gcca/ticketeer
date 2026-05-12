#pragma once

#include <drogon/HttpController.h>

#include "ticketeer/handling/auth/middlewares.hpp"

namespace ticketeer {

class Dashboard : public drogon::HttpController<Dashboard> {
public:
  METHOD_LIST_BEGIN
  METHOD_ADD(Dashboard::IndexGet, "", drogon::Get,
             "ticketeer::handling::auth::middlewares::LogInRequired");
  METHOD_LIST_END

  void
  IndexGet(const drogon::HttpRequestPtr &req,
           std::function<void(const drogon::HttpResponsePtr &)> &&callback);
};

} // namespace ticketeer
