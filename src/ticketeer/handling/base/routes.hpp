#pragma once

#include <drogon/HttpController.h>

namespace ticketeer {

class Base : public drogon::HttpController<Base> {
public:
  METHOD_LIST_BEGIN
  ADD_METHOD_TO(Base::Index, "/", drogon::Get);
  ADD_METHOD_TO(Base::Index, "/ticketeer/", drogon::Get);
  ADD_METHOD_TO(Base::Healthcheck, "/ticketeer/healthcheck", drogon::Get);
  METHOD_LIST_END

  void Index(const drogon::HttpRequestPtr &req,
             std::function<void(const drogon::HttpResponsePtr &)> &&callback);

  void Healthcheck(
      const drogon::HttpRequestPtr &req,
      std::function<void(const drogon::HttpResponsePtr &)> &&callback);
};

} // namespace ticketeer
