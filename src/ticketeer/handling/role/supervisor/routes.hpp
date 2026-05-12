#pragma once

#include <drogon/HttpController.h>

#include "ticketeer/handling/auth/middlewares.hpp"
#include "ticketeer/handling/role/supervisor/middlewares.hpp"

namespace ticketeer {

class Supervisor : public drogon::HttpController<Supervisor> {
public:
  METHOD_LIST_BEGIN
  METHOD_ADD(Supervisor::HomeGet, "", drogon::Get,
             "ticketeer::handling::auth::middlewares::LogInRequired",
             "ticketeer::handling::role::supervisor::middlewares::"
             "RoleSupervisorRequired");
  METHOD_ADD(Supervisor::TicketListGet, "/ticket/list", drogon::Get,
             "ticketeer::handling::auth::middlewares::LogInRequired",
             "ticketeer::handling::role::supervisor::middlewares::"
             "RoleSupervisorRequired");
  METHOD_ADD(Supervisor::TicketCreateGet, "/ticket/create", drogon::Get,
             "ticketeer::handling::auth::middlewares::LogInRequired",
             "ticketeer::handling::role::supervisor::middlewares::"
             "RoleSupervisorRequired");
  METHOD_ADD(Supervisor::TicketCreatePost, "/ticket/create", drogon::Post,
             "ticketeer::handling::auth::middlewares::LogInRequired",
             "ticketeer::handling::role::supervisor::middlewares::"
             "RoleSupervisorRequired");
  METHOD_ADD(Supervisor::TicketDetailsGet, "/ticket/{ticket_id}/details",
             drogon::Get,
             "ticketeer::handling::auth::middlewares::LogInRequired",
             "ticketeer::handling::role::supervisor::middlewares::"
             "RoleSupervisorRequired");
  METHOD_ADD(Supervisor::TicketStatusPost, "/ticket/{ticket_id}/status",
             drogon::Post,
             "ticketeer::handling::auth::middlewares::LogInRequired",
             "ticketeer::handling::role::supervisor::middlewares::"
             "RoleSupervisorRequired");
  METHOD_ADD(Supervisor::TicketDepartmentPost, "/ticket/{ticket_id}/department",
             drogon::Post,
             "ticketeer::handling::auth::middlewares::LogInRequired",
             "ticketeer::handling::role::supervisor::middlewares::"
             "RoleSupervisorRequired");
  METHOD_ADD(Supervisor::TicketAssignedToPost,
             "/ticket/{ticket_id}/assigned_to", drogon::Post,
             "ticketeer::handling::auth::middlewares::LogInRequired",
             "ticketeer::handling::role::supervisor::middlewares::"
             "RoleSupervisorRequired");
  METHOD_ADD(Supervisor::TicketRequestTypePost,
             "/ticket/{ticket_id}/request_type", drogon::Post,
             "ticketeer::handling::auth::middlewares::LogInRequired",
             "ticketeer::handling::role::supervisor::middlewares::"
             "RoleSupervisorRequired");
  METHOD_ADD(Supervisor::TicketPriorityPost, "/ticket/{ticket_id}/priority",
             drogon::Post,
             "ticketeer::handling::auth::middlewares::LogInRequired",
             "ticketeer::handling::role::supervisor::middlewares::"
             "RoleSupervisorRequired");
  METHOD_ADD(Supervisor::TicketActivityListGet,
             "/ticket/{ticket_id}/activity/list", drogon::Get,
             "ticketeer::handling::auth::middlewares::LogInRequired",
             "ticketeer::handling::role::supervisor::middlewares::"
             "RoleSupervisorRequired");
  METHOD_ADD(Supervisor::TicketActivityCreateMessagePost,
             "/ticket/{ticket_id}/activity/create/message", drogon::Post,
             "ticketeer::handling::auth::middlewares::LogInRequired",
             "ticketeer::handling::role::supervisor::middlewares::"
             "RoleSupervisorRequired");
  METHOD_ADD(Supervisor::TicketActivityAttachmentCreatePost,
             "/ticket/{ticket_id}/activity/{activity_id}/attachment/create",
             drogon::Post,
             "ticketeer::handling::auth::middlewares::LogInRequired",
             "ticketeer::handling::role::supervisor::middlewares::"
             "RoleSupervisorRequired");
  METHOD_ADD(Supervisor::TicketActivityAttachmentDownloadGet,
             "/ticket/{ticket_id}/activity/{activity_id}/attachment/"
             "{attachment_id}/download",
             drogon::Get,
             "ticketeer::handling::auth::middlewares::LogInRequired",
             "ticketeer::handling::role::supervisor::middlewares::"
             "RoleSupervisorRequired");
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

  void TicketStatusPost(
      const drogon::HttpRequestPtr &req,
      std::function<void(const drogon::HttpResponsePtr &)> &&callback,
      const std::string &ticket_id);

  void TicketDepartmentPost(
      const drogon::HttpRequestPtr &req,
      std::function<void(const drogon::HttpResponsePtr &)> &&callback,
      const std::string &ticket_id);

  void TicketAssignedToPost(
      const drogon::HttpRequestPtr &req,
      std::function<void(const drogon::HttpResponsePtr &)> &&callback,
      const std::string &ticket_id);

  void TicketRequestTypePost(
      const drogon::HttpRequestPtr &req,
      std::function<void(const drogon::HttpResponsePtr &)> &&callback,
      const std::string &ticket_id);

  void TicketPriorityPost(
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
