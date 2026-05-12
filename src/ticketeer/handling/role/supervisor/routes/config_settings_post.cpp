#include "ticketeer/handling/role/supervisor/routes.hpp"
#include "ticketeer/handling/role/supervisor/routes/common.hpp"

namespace ticketeer {

using namespace ticketeer::handling::role::supervisor::routes::common;

void Supervisor::ConfigSettingsPost(const drogon::HttpRequestPtr &req,
                                    Callback &&callback) {
  const auto token = req->getCookie("token");

  const auto default_status_id = req->getParameter("default_status_id");
  const auto default_assigned_to_id =
      req->getParameter("default_assigned_to_id");
  const auto default_ticket_priority_id =
      req->getParameter("default_ticket_priority_id");
  const auto assigned_status_id = req->getParameter("assigned_status_id");
  const auto system_profile_id = req->getParameter("system_profile_id");
  const auto ticket_body_maxlength = req->getParameter("ticket_body_maxlength");
  const auto ticket_activity_body_maxlength =
      req->getParameter("ticket_activity_body_maxlength");
  const auto ticket_due_delta = req->getParameter("ticket_due_delta");
  const auto ticket_attachment_maxsize =
      req->getParameter("ticket_attachment_maxsize");
  const auto ticket_activity_attachment_maxsize =
      req->getParameter("ticket_activity_attachment_maxsize");

  if (default_status_id.empty() || default_assigned_to_id.empty() ||
      default_ticket_priority_id.empty() || assigned_status_id.empty() ||
      system_profile_id.empty() || ticket_body_maxlength.empty() ||
      ticket_activity_body_maxlength.empty() || ticket_due_delta.empty() ||
      ticket_attachment_maxsize.empty() ||
      ticket_activity_attachment_maxsize.empty())
    return BadRequest(callback, "Missing required fields: default_status_id, "
                                "default_assigned_to_id, "
                                "default_ticket_priority_id, "
                                "assigned_status_id, system_profile_id, "
                                "ticket_body_maxlength, "
                                "ticket_activity_body_maxlength, "
                                "ticket_due_delta, ticket_attachment_maxsize, "
                                "ticket_activity_attachment_maxsize");

  const auto parsed_ticket_body_maxlength =
      ParsePositiveInt(ticket_body_maxlength);
  if (!parsed_ticket_body_maxlength)
    return BadRequest(callback,
                      "Invalid ticket_body_maxlength: must be a positive "
                      "integer");
  const auto parsed_activity_body_maxlength =
      ParsePositiveInt(ticket_activity_body_maxlength);
  if (!parsed_activity_body_maxlength)
    return BadRequest(callback,
                      "Invalid ticket_activity_body_maxlength: must be a "
                      "positive integer");
  const auto parsed_ticket_due_delta = ParsePositiveInt(ticket_due_delta);
  if (!parsed_ticket_due_delta)
    return BadRequest(callback,
                      "Invalid ticket_due_delta: must be a positive integer");
  const auto parsed_ticket_attachment_maxsize =
      ParsePositiveInt(ticket_attachment_maxsize);
  if (!parsed_ticket_attachment_maxsize)
    return BadRequest(callback, "Invalid ticket_attachment_maxsize: must be a "
                                "positive integer");
  const auto parsed_activity_attachment_maxsize =
      ParsePositiveInt(ticket_activity_attachment_maxsize);
  if (!parsed_activity_attachment_maxsize)
    return BadRequest(callback,
                      "Invalid ticket_activity_attachment_maxsize: must be a "
                      "positive integer");

  sqlite3 *db = Connect();
  if (!db)
    return BadRequest(callback, "Database unavailable",
                      drogon::k503ServiceUnavailable);

  const auto profile = FetchProfile(db, token);
  if (!profile) {
    sqlite3_close(db);
    return BadRequest(callback, "Invalid or expired token",
                      drogon::k401Unauthorized);
  }

  if (!UpsertDefaultSetting(
          db, default_status_id, default_assigned_to_id,
          default_ticket_priority_id, assigned_status_id, system_profile_id,
          *parsed_ticket_body_maxlength, *parsed_activity_body_maxlength,
          *parsed_ticket_due_delta, *parsed_ticket_attachment_maxsize,
          *parsed_activity_attachment_maxsize)) {
    sqlite3_close(db);
    return BadRequest(callback, "Failed to save settings");
  }

  RenderSettingsConfig(callback, db);
  sqlite3_close(db);
}

} // namespace ticketeer
