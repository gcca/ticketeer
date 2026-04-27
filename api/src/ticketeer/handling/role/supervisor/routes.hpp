#pragma once

#include <drogon/HttpController.h>

#include "ticketeer/handling/auth/middlewares.hpp"
#include "ticketeer/handling/role/supervisor/middlewares.hpp"

namespace ticketeer::api::role {

class Supervisor : public drogon::HttpController<Supervisor> {
public:
  METHOD_LIST_BEGIN
  METHOD_ADD(Supervisor::TicketList, "/v1/ticket/list", drogon::Get,
             "ticketeer::api::auth::middlewares::LogInRequired",
             "ticketeer::api::role::supervisor::middlewares::"
             "RoleSupervisorRequired");
  METHOD_ADD(Supervisor::TicketCreate, "/v1/ticket/create", drogon::Post,
             "ticketeer::api::auth::middlewares::LogInRequired",
             "ticketeer::api::role::supervisor::middlewares::"
             "RoleSupervisorRequired");
  METHOD_ADD(Supervisor::TicketDetails, "/v1/ticket/{ticket_id}/details",
             drogon::Get, "ticketeer::api::auth::middlewares::LogInRequired",
             "ticketeer::api::role::supervisor::middlewares::"
             "RoleSupervisorRequired");
  METHOD_ADD(Supervisor::TicketActivityList,
             "/v1/ticket/{ticket_id}/activity/list", drogon::Get,
             "ticketeer::api::auth::middlewares::LogInRequired",
             "ticketeer::api::role::supervisor::middlewares::"
             "RoleSupervisorRequired");
  METHOD_ADD(Supervisor::TicketActivityCreateMessage,
             "/v1/ticket/{ticket_id}/activity/create/message", drogon::Post,
             "ticketeer::api::auth::middlewares::LogInRequired",
             "ticketeer::api::role::supervisor::middlewares::"
             "RoleSupervisorRequired");
  METHOD_LIST_END

  void
  TicketList(const drogon::HttpRequestPtr &req,
             std::function<void(const drogon::HttpResponsePtr &)> &&callback);

  void
  TicketCreate(const drogon::HttpRequestPtr &req,
               std::function<void(const drogon::HttpResponsePtr &)> &&callback);

  void
  TicketDetails(const drogon::HttpRequestPtr &req,
                std::function<void(const drogon::HttpResponsePtr &)> &&callback,
                const std::string &ticket_id);

  void TicketActivityList(
      const drogon::HttpRequestPtr &req,
      std::function<void(const drogon::HttpResponsePtr &)> &&callback,
      const std::string &ticket_id);

  void TicketActivityCreateMessage(
      const drogon::HttpRequestPtr &req,
      std::function<void(const drogon::HttpResponsePtr &)> &&callback,
      const std::string &ticket_id);
};

} // namespace ticketeer::api::role
