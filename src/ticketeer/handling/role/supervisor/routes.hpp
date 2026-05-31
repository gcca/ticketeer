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
  METHOD_ADD(Supervisor::TicketAssignedToPost,
             "/ticket/{ticket_id}/assigned_to", drogon::Post,
             "ticketeer::handling::auth::middlewares::LogInRequired",
             "ticketeer::handling::role::supervisor::middlewares::"
             "RoleSupervisorRequired");
  METHOD_ADD(Supervisor::TicketPriorityPost, "/ticket/{ticket_id}/priority",
             drogon::Post,
             "ticketeer::handling::auth::middlewares::LogInRequired",
             "ticketeer::handling::role::supervisor::middlewares::"
             "RoleSupervisorRequired");
  METHOD_ADD(Supervisor::TicketDueDatePost, "/ticket/{ticket_id}/due_date",
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
  METHOD_ADD(Supervisor::ConfigTicketStatusListGet,
             "/config/ticket-status/list", drogon::Get,
             "ticketeer::handling::auth::middlewares::LogInRequired",
             "ticketeer::handling::role::supervisor::middlewares::"
             "RoleSupervisorRequired");
  METHOD_ADD(Supervisor::ConfigTicketStatusCreatePost,
             "/config/ticket-status/save", drogon::Post,
             "ticketeer::handling::auth::middlewares::LogInRequired",
             "ticketeer::handling::role::supervisor::middlewares::"
             "RoleSupervisorRequired");
  METHOD_ADD(Supervisor::ConfigTicketStatusUpdatePost,
             "/config/ticket-status/{ticket_status_id}/save", drogon::Post,
             "ticketeer::handling::auth::middlewares::LogInRequired",
             "ticketeer::handling::role::supervisor::middlewares::"
             "RoleSupervisorRequired");
  METHOD_ADD(Supervisor::ConfigTicketStatusDeletePost,
              "/config/ticket-status/{ticket_status_id}/void", drogon::Post,
              "ticketeer::handling::auth::middlewares::LogInRequired",
              "ticketeer::handling::role::supervisor::middlewares::"
              "RoleSupervisorRequired");
  METHOD_ADD(Supervisor::ConfigTicketPriorityListGet,
              "/config/ticket-priority/list", drogon::Get,
              "ticketeer::handling::auth::middlewares::LogInRequired",
              "ticketeer::handling::role::supervisor::middlewares::"
              "RoleSupervisorRequired");
  METHOD_ADD(Supervisor::ConfigTicketPriorityCreatePost,
              "/config/ticket-priority/save", drogon::Post,
              "ticketeer::handling::auth::middlewares::LogInRequired",
              "ticketeer::handling::role::supervisor::middlewares::"
              "RoleSupervisorRequired");
  METHOD_ADD(Supervisor::ConfigTicketPriorityUpdatePost,
              "/config/ticket-priority/{ticket_priority_id}/save", drogon::Post,
              "ticketeer::handling::auth::middlewares::LogInRequired",
              "ticketeer::handling::role::supervisor::middlewares::"
              "RoleSupervisorRequired");
  METHOD_ADD(Supervisor::ConfigTicketPriorityDeletePost,
              "/config/ticket-priority/{ticket_priority_id}/void", drogon::Post,
              "ticketeer::handling::auth::middlewares::LogInRequired",
              "ticketeer::handling::role::supervisor::middlewares::"
              "RoleSupervisorRequired");
  METHOD_ADD(Supervisor::ConfigSettingsGet, "/config/settings", drogon::Get,
             "ticketeer::handling::auth::middlewares::LogInRequired",
             "ticketeer::handling::role::supervisor::middlewares::"
             "RoleSupervisorRequired");
  METHOD_ADD(Supervisor::ConfigSettingsPost, "/config/settings/save",
             drogon::Post,
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

  void TicketAssignedToPost(
      const drogon::HttpRequestPtr &req,
      std::function<void(const drogon::HttpResponsePtr &)> &&callback,
      const std::string &ticket_id);

  void TicketPriorityPost(
      const drogon::HttpRequestPtr &req,
      std::function<void(const drogon::HttpResponsePtr &)> &&callback,
      const std::string &ticket_id);

  void TicketDueDatePost(
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

  void ConfigTicketStatusListGet(
      const drogon::HttpRequestPtr &req,
      std::function<void(const drogon::HttpResponsePtr &)> &&callback);

  void ConfigTicketStatusCreatePost(
      const drogon::HttpRequestPtr &req,
      std::function<void(const drogon::HttpResponsePtr &)> &&callback);

  void ConfigTicketStatusUpdatePost(
      const drogon::HttpRequestPtr &req,
      std::function<void(const drogon::HttpResponsePtr &)> &&callback,
      const std::string &ticket_status_id);

  void ConfigTicketStatusDeletePost(
      const drogon::HttpRequestPtr &req,
      std::function<void(const drogon::HttpResponsePtr &)> &&callback,
      const std::string &ticket_status_id);

  void ConfigTicketPriorityListGet(
      const drogon::HttpRequestPtr &req,
      std::function<void(const drogon::HttpResponsePtr &)> &&callback);

  void ConfigTicketPriorityCreatePost(
      const drogon::HttpRequestPtr &req,
      std::function<void(const drogon::HttpResponsePtr &)> &&callback);

  void ConfigTicketPriorityUpdatePost(
      const drogon::HttpRequestPtr &req,
      std::function<void(const drogon::HttpResponsePtr &)> &&callback,
      const std::string &ticket_priority_id);

  void ConfigTicketPriorityDeletePost(
      const drogon::HttpRequestPtr &req,
      std::function<void(const drogon::HttpResponsePtr &)> &&callback,
      const std::string &ticket_priority_id);

  void ConfigSettingsGet(
      const drogon::HttpRequestPtr &req,
      std::function<void(const drogon::HttpResponsePtr &)> &&callback);

  void ConfigSettingsPost(
      const drogon::HttpRequestPtr &req,
      std::function<void(const drogon::HttpResponsePtr &)> &&callback);
};

} // namespace ticketeer
