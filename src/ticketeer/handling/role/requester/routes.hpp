#pragma once

#include <drogon/HttpController.h>

#include "ticketeer/handling/auth/middlewares.hpp"
#include "ticketeer/handling/role/requester/middlewares.hpp"

namespace ticketeer {

class Requester : public drogon::HttpController<Requester> {
public:
  METHOD_LIST_BEGIN
  METHOD_ADD(Requester::TicketList, "/ticket/list", drogon::Get,
             "ticketeer::handling::auth::middlewares::LogInRequired",
             "ticketeer::handling::role::requester::middlewares::"
             "RoleRequesterRequired");
  METHOD_ADD(Requester::TicketCreate, "/ticket/create", drogon::Post,
             "ticketeer::handling::auth::middlewares::LogInRequired",
             "ticketeer::handling::role::requester::middlewares::"
             "RoleRequesterRequired");
  METHOD_ADD(Requester::TicketDetails, "/ticket/{ticket_id}/details",
             drogon::Get, "ticketeer::handling::auth::middlewares::LogInRequired",
             "ticketeer::handling::role::requester::middlewares::"
             "RoleRequesterRequired");
  METHOD_ADD(Requester::TicketActivityList,
             "/ticket/{ticket_id}/activity/list", drogon::Get,
             "ticketeer::handling::auth::middlewares::LogInRequired",
             "ticketeer::handling::role::requester::middlewares::"
             "RoleRequesterRequired");
  METHOD_ADD(Requester::TicketActivityCreateMessage,
             "/ticket/{ticket_id}/activity/create/message", drogon::Post,
             "ticketeer::handling::auth::middlewares::LogInRequired",
             "ticketeer::handling::role::requester::middlewares::"
             "RoleRequesterRequired");
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

} // namespace ticketeer
