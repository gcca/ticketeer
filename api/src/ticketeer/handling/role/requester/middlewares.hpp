#pragma once

#include <drogon/HttpMiddleware.h>

namespace ticketeer::api::role::requester::middlewares {

class RoleRequesterRequired
    : public drogon::HttpMiddleware<RoleRequesterRequired> {
public:
  void invoke(const drogon::HttpRequestPtr &req,
              drogon::MiddlewareNextCallback &&nextCb,
              drogon::MiddlewareCallback &&mcb) override;

private:
  static void Reject(const drogon::MiddlewareCallback &mcb, const char *msg,
                     drogon::HttpStatusCode code);
};

} // namespace ticketeer::api::role::requester::middlewares
