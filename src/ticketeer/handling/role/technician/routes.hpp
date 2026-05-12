#pragma once

#include <drogon/HttpController.h>

namespace ticketeer {

class Technician : public drogon::HttpController<Technician> {
public:
  METHOD_LIST_BEGIN
  METHOD_ADD(Technician::HomeGet, "", drogon::Get,
             "ticketeer::handling::auth::middlewares::LogInRequired",
             "ticketeer::handling::role::technician::middlewares::"
             "RoleTechnicianRequired");
  METHOD_ADD(Technician::TicketListGet, "/ticket/list", drogon::Get,
             "ticketeer::handling::auth::middlewares::LogInRequired",
             "ticketeer::handling::role::technician::middlewares::"
             "RoleTechnicianRequired");
  METHOD_ADD(Technician::TicketCreateGet, "/ticket/create", drogon::Get,
             "ticketeer::handling::auth::middlewares::LogInRequired",
             "ticketeer::handling::role::technician::middlewares::"
             "RoleTechnicianRequired");
  METHOD_ADD(Technician::TicketCreatePost, "/ticket/create", drogon::Post,
             "ticketeer::handling::auth::middlewares::LogInRequired",
             "ticketeer::handling::role::technician::middlewares::"
             "RoleTechnicianRequired");
  METHOD_ADD(Technician::TicketDetailsGet, "/ticket/{ticket_id}/details",
             drogon::Get,
             "ticketeer::handling::auth::middlewares::LogInRequired",
             "ticketeer::handling::role::technician::middlewares::"
             "RoleTechnicianRequired");
  METHOD_ADD(Technician::TicketActivityListGet,
             "/ticket/{ticket_id}/activity/list", drogon::Get,
             "ticketeer::handling::auth::middlewares::LogInRequired",
             "ticketeer::handling::role::technician::middlewares::"
             "RoleTechnicianRequired");
  METHOD_ADD(Technician::TicketActivityCreateMessagePost,
             "/ticket/{ticket_id}/activity/create/message", drogon::Post,
             "ticketeer::handling::auth::middlewares::LogInRequired",
             "ticketeer::handling::role::technician::middlewares::"
             "RoleTechnicianRequired");
  METHOD_ADD(Technician::TicketActivityAttachmentCreatePost,
             "/ticket/{ticket_id}/activity/{activity_id}/attachment/create",
             drogon::Post,
             "ticketeer::handling::auth::middlewares::LogInRequired",
             "ticketeer::handling::role::technician::middlewares::"
             "RoleTechnicianRequired");
  METHOD_ADD(Technician::TicketActivityAttachmentDownloadGet,
             "/ticket/{ticket_id}/activity/{activity_id}/attachment/"
             "{attachment_id}/download",
             drogon::Get,
             "ticketeer::handling::auth::middlewares::LogInRequired",
             "ticketeer::handling::role::technician::middlewares::"
             "RoleTechnicianRequired");
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
