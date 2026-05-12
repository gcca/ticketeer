#pragma once

#include <drogon/HttpController.h>

#include "ticketeer/handling/auth/middlewares.hpp"
#include "ticketeer/handling/role/requester/middlewares.hpp"

namespace ticketeer {

class Requester : public drogon::HttpController<Requester> {
public:
  METHOD_LIST_BEGIN
  METHOD_ADD(Requester::HomeGet, "", drogon::Get,
             "ticketeer::handling::auth::middlewares::LogInRequired",
             "ticketeer::handling::role::requester::middlewares::"
             "RoleRequesterRequired");
  METHOD_ADD(Requester::TicketListGet, "/ticket/list", drogon::Get,
             "ticketeer::handling::auth::middlewares::LogInRequired",
             "ticketeer::handling::role::requester::middlewares::"
             "RoleRequesterRequired");
  METHOD_ADD(Requester::TicketCreateGet, "/ticket/create", drogon::Get,
             "ticketeer::handling::auth::middlewares::LogInRequired",
             "ticketeer::handling::role::requester::middlewares::"
             "RoleRequesterRequired");
  METHOD_ADD(Requester::TicketCreatePost, "/ticket/create", drogon::Post,
             "ticketeer::handling::auth::middlewares::LogInRequired",
             "ticketeer::handling::role::requester::middlewares::"
             "RoleRequesterRequired");
  METHOD_ADD(Requester::TicketDetailsGet, "/ticket/{ticket_id}/details",
             drogon::Get,
             "ticketeer::handling::auth::middlewares::LogInRequired",
             "ticketeer::handling::role::requester::middlewares::"
             "RoleRequesterRequired");
  METHOD_ADD(Requester::TicketActivityListGet,
             "/ticket/{ticket_id}/activity/list", drogon::Get,
             "ticketeer::handling::auth::middlewares::LogInRequired",
             "ticketeer::handling::role::requester::middlewares::"
             "RoleRequesterRequired");
  METHOD_ADD(Requester::TicketActivityCreateMessagePost,
             "/ticket/{ticket_id}/activity/create/message", drogon::Post,
             "ticketeer::handling::auth::middlewares::LogInRequired",
             "ticketeer::handling::role::requester::middlewares::"
             "RoleRequesterRequired");
  METHOD_ADD(Requester::TicketActivityAttachmentCreatePost,
             "/ticket/{ticket_id}/activity/{activity_id}/attachment/create",
             drogon::Post,
             "ticketeer::handling::auth::middlewares::LogInRequired",
             "ticketeer::handling::role::requester::middlewares::"
             "RoleRequesterRequired");
  METHOD_ADD(Requester::TicketActivityAttachmentDownloadGet,
             "/ticket/{ticket_id}/activity/{activity_id}/attachment/"
             "{attachment_id}/download",
             drogon::Get,
             "ticketeer::handling::auth::middlewares::LogInRequired",
             "ticketeer::handling::role::requester::middlewares::"
             "RoleRequesterRequired");
  METHOD_LIST_END

  void HomeGet(const drogon::HttpRequestPtr &req,
               std::function<void(const drogon::HttpResponsePtr &)> &&callback);

  void TicketListGet(
      const drogon::HttpRequestPtr &req,
      std::function<void(const drogon::HttpResponsePtr &)> &&callback);

  void TicketCreateGet(
      const drogon::HttpRequestPtr &req,
      std::function<void(const drogon::HttpResponsePtr &)> &&callback);

  void TicketCreatePost(
      const drogon::HttpRequestPtr &req,
      std::function<void(const drogon::HttpResponsePtr &)> &&callback);

  void TicketDetailsGet(
      const drogon::HttpRequestPtr &req,
      std::function<void(const drogon::HttpResponsePtr &)> &&callback,
      const std::string &ticket_id);

  void TicketActivityListGet(
      const drogon::HttpRequestPtr &req,
      std::function<void(const drogon::HttpResponsePtr &)> &&callback,
      const std::string &ticket_id);

  void TicketActivityCreateMessagePost(
      const drogon::HttpRequestPtr &req,
      std::function<void(const drogon::HttpResponsePtr &)> &&callback,
      const std::string &ticket_id);

  void TicketActivityAttachmentCreatePost(
      const drogon::HttpRequestPtr &req,
      std::function<void(const drogon::HttpResponsePtr &)> &&callback,
      const std::string &ticket_id, const std::string &activity_id);

  void TicketActivityAttachmentDownloadGet(
      const drogon::HttpRequestPtr &req,
      std::function<void(const drogon::HttpResponsePtr &)> &&callback,
      const std::string &ticket_id, const std::string &activity_id,
      const std::string &attachment_id);
};

} // namespace ticketeer
