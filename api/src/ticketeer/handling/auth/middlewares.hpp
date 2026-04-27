#pragma once

#include <drogon/HttpMiddleware.h>

namespace ticketeer::api::auth::middlewares {

class LogInRequired : public drogon::HttpMiddleware<LogInRequired> {
public:
  void invoke(const drogon::HttpRequestPtr &req,
              drogon::MiddlewareNextCallback &&nextCb,
              drogon::MiddlewareCallback &&mcb) override;

private:
  static void Reject(const drogon::MiddlewareCallback &mcb, const char *msg,
                     drogon::HttpStatusCode code);
};

} // namespace ticketeer::api::auth::middlewares
