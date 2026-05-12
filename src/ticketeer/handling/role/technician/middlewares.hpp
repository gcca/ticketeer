#pragma once

#include <drogon/HttpMiddleware.h>

namespace ticketeer::handling::role::technician::middlewares {

class RoleTechnicianRequired
    : public drogon::HttpMiddleware<RoleTechnicianRequired> {
public:
  void invoke(const drogon::HttpRequestPtr &req,
              drogon::MiddlewareNextCallback &&nextCb,
              drogon::MiddlewareCallback &&mcb) override;

private:
  static void Reject(const drogon::MiddlewareCallback &mcb, const char *msg,
                     drogon::HttpStatusCode code);
};

} // namespace ticketeer::handling::role::technician::middlewares
