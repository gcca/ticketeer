#include "routes.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <drogon/MultiPart.h>
#include <json/json.h>
#include <libpq-fe.h>
#include <quill/Frontend.h>
#include <quill/LogMacros.h>

#include "ticketeer/core/conf.hpp"
#include "ticketeer/handling/role/common.hpp"

namespace {

using Callback = std::function<void(const drogon::HttpResponsePtr &)>;

enum class TicketLookupStatus { Found, NotFound, QueryFailed };

struct TicketLookup {
  TicketLookupStatus status;
  Json::Value ticket;
};

struct AttachmentFile {
  std::string file_path;
  std::string file_name;
  std::string mime_type;
};

inline constexpr std::size_t MaxAttachmentSize = 5 * 1024 * 1024;

inline void BadRequest(const Callback &callback, const char *msg,
                       drogon::HttpStatusCode code = drogon::k400BadRequest) {
  Json::Value error;
  error["message"] = msg;
  auto resp = drogon::HttpResponse::newHttpJsonResponse(error);
  resp->setStatusCode(code);
  callback(resp);
}

[[nodiscard]] inline PGconn *ConnectDB() {
  return PQconnectdb(ticketeer::core::conf::settings.DB_URL.c_str());
}

[[nodiscard]] inline std::string
AttachmentDownloadUrl(const std::string &ticket_id,
                      const std::string &activity_id,
                      const std::string &attachment_id) {
  return "/ticketeer/api/role/supervisor/v1/ticket/" + ticket_id +
         "/activity/" + activity_id + "/attachment/" + attachment_id +
         "/download";
}

[[nodiscard]] inline std::string ToLower(std::string value) {
  std::transform(
      value.begin(), value.end(), value.begin(),
      [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return value;
}

[[nodiscard]] inline std::string InferMimeType(const std::string &file_name) {
  const auto ext =
      ToLower(std::filesystem::path(file_name).extension().string());
  static const std::unordered_map<std::string, std::string> mime_types = {
      {".txt", "text/plain"},
      {".csv", "text/csv"},
      {".html", "text/html"},
      {".htm", "text/html"},
      {".json", "application/json"},
      {".xml", "application/xml"},
      {".pdf", "application/pdf"},
      {".png", "image/png"},
      {".jpg", "image/jpeg"},
      {".jpeg", "image/jpeg"},
      {".gif", "image/gif"},
      {".webp", "image/webp"},
      {".svg", "image/svg+xml"},
      {".zip", "application/zip"},
      {".doc", "application/msword"},
      {".docx", "application/"
                "vnd.openxmlformats-officedocument.wordprocessingml.document"},
      {".xls", "application/vnd.ms-excel"},
      {".xlsx",
       "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet"},
      {".ppt", "application/vnd.ms-powerpoint"},
      {".pptx",
       "application/"
       "vnd.openxmlformats-officedocument.presentationml.presentation"}};
  const auto it = mime_types.find(ext);
  return it == mime_types.end() ? "application/octet-stream" : it->second;
}

[[nodiscard]] inline bool WriteFile(const std::filesystem::path &path,
                                    std::string_view content) {
  std::error_code ec;
  std::filesystem::create_directories(path.parent_path(), ec);
  if (ec) {
    return false;
  }

  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  if (!out) {
    return false;
  }
  out.write(content.data(), static_cast<std::streamsize>(content.size()));
  return out.good();
}

[[nodiscard]] inline std::filesystem::path
AttachmentAbsolutePath(const std::string &file_path,
                       const std::string &file_name) {
  return std::filesystem::current_path() /
         ticketeer::core::conf::settings.UPLOAD_DIR /
         std::filesystem::path(file_path) /
         std::filesystem::path(file_name).filename();
}

[[nodiscard]] inline std::optional<std::string>
FetchProfileId(quill::Logger *logger, PGconn *pg, const std::string &token) {
  const char *params[] = {token.c_str()};
  PGresult *res = PQexecParams(pg,
                               R"SQL(
SELECT p.id::text
  FROM helpdesk.profile p
  JOIN auth.session s
    ON s.user_id = p.user_id
 WHERE s.token = $1
   AND s.expires_at > now()
   AND p.role = 'supervisor'::helpdesk.role
)SQL",
                               1, nullptr, params, nullptr, nullptr, 0);

  const auto status = PQresultStatus(res);
  if (status != PGRES_TUPLES_OK) {
    LOGJ_DEBUG(logger, "[supervisor] user query error",
               PQresultErrorMessage(res));
    PQclear(res);
    return std::nullopt;
  }

  if (PQntuples(res) != 1) {
    LOGJ_DEBUG(logger, "[supervisor] user not found");
    PQclear(res);
    return std::nullopt;
  }

  std::string user_id = PQgetvalue(res, 0, 0);
  PQclear(res);
  return user_id;
}

[[nodiscard]] inline std::optional<std::string>
FetchDefaultStatusId(quill::Logger *logger, PGconn *pg) {
  PGresult *status_res = PQexecParams(pg,
                                      R"SQL(
SELECT default_status_id::text
  FROM helpdesk.setting
 WHERE name = 'default'
)SQL",
                                      0, nullptr, nullptr, nullptr, nullptr, 0);
  if (PQresultStatus(status_res) != PGRES_TUPLES_OK ||
      PQntuples(status_res) != 1) {
    LOGJ_DEBUG(logger, "[supervisor] default status query error",
               PQresultErrorMessage(status_res));
    PQclear(status_res);
    return std::nullopt;
  }

  std::string default_status_id = PQgetvalue(status_res, 0, 0);
  PQclear(status_res);
  return default_status_id;
}

[[nodiscard]] inline std::optional<std::string>
FetchDefaultAssignedToId(quill::Logger *logger, PGconn *pg) {
  PGresult *res = PQexecParams(pg,
                               R"SQL(
SELECT default_assigned_to_id::text
  FROM helpdesk.setting
 WHERE name = 'default'
)SQL",
                               0, nullptr, nullptr, nullptr, nullptr, 0);
  if (PQresultStatus(res) != PGRES_TUPLES_OK || PQntuples(res) != 1) {
    LOGJ_DEBUG(logger, "[supervisor] default assigned to query error",
               PQresultErrorMessage(res));
    PQclear(res);
    return std::nullopt;
  }

  std::string default_assigned_to_id = PQgetvalue(res, 0, 0);
  PQclear(res);
  return default_assigned_to_id;
}

[[nodiscard]] inline std::optional<Json::Value>
CreateTicket(quill::Logger *logger, PGconn *pg, const std::string &profile_id,
             const Json::Value &request_type_id,
             const Json::Value &department_id, const Json::Value &priority_id,
             const std::string &default_status_id,
             const std::string &default_assigned_to_id,
             const Json::Value &description, const Json::Value &due_date) {
  std::string request_type_str = request_type_id.asString();
  std::string department_str = department_id.asString();
  std::string priority_str = priority_id.asString();
  std::string description_str = description.asString();
  std::string due_date_str = due_date.isString() ? due_date.asString() : "";

  std::vector<const char *> params = {
      request_type_str.c_str(),  profile_id.c_str(),
      department_str.c_str(),    priority_str.c_str(),
      default_status_id.c_str(), default_assigned_to_id.c_str(),
      description_str.c_str()};
  std::string query =
      "INSERT INTO helpdesk.ticket (request_type_id, requester_id, "
      "department_id, priority_id, status_id, assigned_to_id, description";
  if (!due_date_str.empty()) {
    query += ", due_date";
    params.push_back(due_date_str.c_str());
  }
  query += ") VALUES ($1::bigint, $2::bigint, $3::bigint, $4::bigint, "
           "$5::bigint, $6::bigint, $7";
  if (!due_date_str.empty()) {
    query += ", $8";
  }
  query += ") RETURNING id::text, description, created_at::text";

  PGresult *insert_res =
      PQexecParams(pg, query.c_str(), static_cast<int>(params.size()), nullptr,
                   params.data(), nullptr, nullptr, 0);
  if (PQresultStatus(insert_res) != PGRES_TUPLES_OK ||
      PQntuples(insert_res) != 1) {
    LOGJ_DEBUG(logger, "[supervisor] insert ticket error",
               PQresultErrorMessage(insert_res));
    PQclear(insert_res);
    return std::nullopt;
  }

  Json::Value ticket;
  ticket["id"] = PQgetvalue(insert_res, 0, 0);
  ticket["description"] = PQgetvalue(insert_res, 0, 1);
  ticket["created_at"] = PQgetvalue(insert_res, 0, 2);
  PQclear(insert_res);
  return ticket;
}

[[nodiscard]] inline TicketLookup
FetchTicketDetails(quill::Logger *logger, PGconn *pg,
                   const std::string &ticket_id) {
  const char *params[] = {ticket_id.c_str()};
  PGresult *res = PQexecParams(pg,
                               R"SQL(
SELECT t.id::text,
       t.request_type_id::text,
       t.requester_id::text,
       t.assigned_to_id::text,
       t.department_id::text,
       t.priority_id::text,
       t.status_id::text,
       t.description,
       t.due_date::text,
       t.created_at::text,
       d.name AS department_name,
       p.display_name AS priority_name,
       ts.display_name AS status_name,
       rt.name AS request_type_name,
       rt.description AS request_type_description,
       pr.username AS requester_username,
       prf.name AS requester_name,
       ap.name AS assigned_to_name,
       au.username AS assigned_to_username,
       au.id::text AS assigned_to_user_id
  FROM helpdesk.ticket t
  JOIN helpdesk.department d
    ON d.id = t.department_id
  JOIN helpdesk.priority p
    ON p.id = t.priority_id
  JOIN helpdesk.ticket_status ts
    ON ts.id = t.status_id
  JOIN helpdesk.request_type rt
    ON rt.id = t.request_type_id
  JOIN helpdesk.profile prf
    ON prf.id = t.requester_id
  JOIN auth."user" pr
    ON pr.id = prf.user_id
  LEFT JOIN helpdesk.profile ap
    ON ap.id = t.assigned_to_id
  LEFT JOIN auth."user" au
    ON au.id = ap.user_id
 WHERE t.id = $1::bigint
)SQL",
                               1, nullptr, params, nullptr, nullptr, 0);

  if (PQresultStatus(res) != PGRES_TUPLES_OK) {
    LOGJ_DEBUG(logger, "[supervisor] ticket details query error",
               PQresultErrorMessage(res));
    PQclear(res);
    return {TicketLookupStatus::QueryFailed, Json::Value{}};
  }

  if (PQntuples(res) != 1) {
    PQclear(res);
    return {TicketLookupStatus::NotFound, Json::Value{}};
  }

  Json::Value ticket;
  ticket["id"] = PQgetvalue(res, 0, 0);
  const char *assigned_to_id = PQgetvalue(res, 0, 3);
  if (assigned_to_id && strlen(assigned_to_id) > 0) {
    ticket["assigned_to"]["id"] = assigned_to_id;
    ticket["assigned_to"]["name"] = PQgetvalue(res, 0, 17);
    ticket["assigned_to"]["user"]["id"] = PQgetvalue(res, 0, 19);
    ticket["assigned_to"]["user"]["username"] = PQgetvalue(res, 0, 18);
  }
  ticket["description"] = PQgetvalue(res, 0, 7);
  const char *due_date = PQgetvalue(res, 0, 8);
  if (due_date && strlen(due_date) > 0) {
    ticket["due_date"] = due_date;
  }
  ticket["created_at"] = PQgetvalue(res, 0, 9);
  ticket["department"]["id"] = PQgetvalue(res, 0, 4);
  ticket["department"]["name"] = PQgetvalue(res, 0, 10);
  ticket["priority"]["id"] = PQgetvalue(res, 0, 5);
  ticket["priority"]["display_name"] = PQgetvalue(res, 0, 11);
  ticket["status"]["id"] = PQgetvalue(res, 0, 6);
  ticket["status"]["display_name"] = PQgetvalue(res, 0, 12);
  ticket["request_type"]["id"] = PQgetvalue(res, 0, 1);
  ticket["request_type"]["name"] = PQgetvalue(res, 0, 13);
  ticket["request_type"]["description"] = PQgetvalue(res, 0, 14);
  ticket["requester"]["name"] = PQgetvalue(res, 0, 16);
  ticket["requester"]["user"]["id"] = PQgetvalue(res, 0, 2);
  ticket["requester"]["user"]["username"] = PQgetvalue(res, 0, 15);

  PQclear(res);
  return {TicketLookupStatus::Found, ticket};
}

[[nodiscard]] inline TicketLookup
FetchTicketActivityList(quill::Logger *logger, PGconn *pg,
                        const std::string &ticket_id) {
  const char *ticket_params[] = {ticket_id.c_str()};
  PGresult *ticket_res =
      PQexecParams(pg,
                   R"SQL(
SELECT id::text
  FROM helpdesk.ticket
 WHERE id = $1::bigint
)SQL",
                   1, nullptr, ticket_params, nullptr, nullptr, 0);

  if (PQresultStatus(ticket_res) != PGRES_TUPLES_OK) {
    LOGJ_DEBUG(logger, "[supervisor] ticket activity owner query error",
               PQresultErrorMessage(ticket_res));
    PQclear(ticket_res);
    return {TicketLookupStatus::QueryFailed, Json::Value{}};
  }

  if (PQntuples(ticket_res) != 1) {
    PQclear(ticket_res);
    return {TicketLookupStatus::NotFound, Json::Value{}};
  }
  PQclear(ticket_res);

  const char *activity_params[] = {ticket_id.c_str()};
  PGresult *activities =
      PQexecParams(pg,
                   R"SQL(
SELECT ta.id::text,
       ta.kind::text,
       ta.body,
       ta.created_at::text,
       p.name AS profile_name
  FROM helpdesk.ticket_activity ta
  JOIN helpdesk.profile p
    ON p.id = ta.profile_id
 WHERE ta.ticket_id = $1::bigint
 ORDER BY ta.created_at DESC,
          ta.id DESC
)SQL",
                   1, nullptr, activity_params, nullptr, nullptr, 0);

  if (PQresultStatus(activities) != PGRES_TUPLES_OK) {
    LOGJ_DEBUG(logger, "[supervisor] ticket activity list query error",
               PQresultErrorMessage(activities));
    PQclear(activities);
    return {TicketLookupStatus::QueryFailed, Json::Value{}};
  }

  Json::Value result = Json::arrayValue;
  for (int i = 0; i < PQntuples(activities); ++i) {
    const std::string activity_id = PQgetvalue(activities, i, 0);
    Json::Value activity;
    activity["id"] = activity_id;
    activity["kind"] = PQgetvalue(activities, i, 1);
    activity["body"] = PQgetvalue(activities, i, 2);
    activity["created_at"] = PQgetvalue(activities, i, 3);
    activity["profile_name"] = PQgetvalue(activities, i, 4);
    activity["attachments"] = Json::arrayValue;

    const char *attachment_params[] = {activity_id.c_str()};
    PGresult *attachments =
        PQexecParams(pg,
                     R"SQL(
SELECT id::text,
       file_name
  FROM helpdesk.ticket_activity_attachment
 WHERE ticket_activity_id = $1::bigint
 ORDER BY created_at ASC,
          id ASC
)SQL",
                     1, nullptr, attachment_params, nullptr, nullptr, 0);

    if (PQresultStatus(attachments) != PGRES_TUPLES_OK) {
      LOGJ_DEBUG(logger, "[supervisor] ticket activity attachment list error",
                 PQresultErrorMessage(attachments));
      PQclear(attachments);
      return {TicketLookupStatus::QueryFailed, Json::Value{}};
    }

    for (int j = 0; j < PQntuples(attachments); ++j) {
      const std::string attachment_id = PQgetvalue(attachments, j, 0);
      Json::Value attachment;
      attachment["id"] = attachment_id;
      attachment["file_name"] = PQgetvalue(attachments, j, 1);
      activity["attachments"].append(attachment);
    }
    PQclear(attachments);

    result.append(activity);
  }
  PQclear(activities);

  return {TicketLookupStatus::Found, result};
}

[[nodiscard]] inline std::optional<Json::Value> CreateTicketActivityMessage(
    quill::Logger *logger, PGconn *pg, const std::string &ticket_id,
    const std::string &profile_id, const std::string &message) {
  const char *params[] = {ticket_id.c_str(), profile_id.c_str(),
                          message.c_str()};
  PGresult *insert_res = PQexecParams(pg,
                                      R"SQL(
INSERT INTO helpdesk.ticket_activity (ticket_id, profile_id, kind, body)
VALUES ($1::bigint, $2::bigint, 'message', $3)
RETURNING id::text, kind::text, body, created_at::text,
         (SELECT name FROM helpdesk.profile WHERE id = $2::bigint) AS profile_name
      )SQL",
                                      3, nullptr, params, nullptr, nullptr, 0);

  if (PQresultStatus(insert_res) != PGRES_TUPLES_OK ||
      PQntuples(insert_res) != 1) {
    LOGJ_DEBUG(logger, "[supervisor] insert activity error",
               PQresultErrorMessage(insert_res));
    PQclear(insert_res);
    return std::nullopt;
  }

  Json::Value activity;
  activity["id"] = PQgetvalue(insert_res, 0, 0);
  activity["kind"] = PQgetvalue(insert_res, 0, 1);
  activity["body"] = PQgetvalue(insert_res, 0, 2);
  activity["created_at"] = PQgetvalue(insert_res, 0, 3);
  activity["profile_name"] = PQgetvalue(insert_res, 0, 4);
  PQclear(insert_res);
  return activity;
}

[[nodiscard]] inline bool TicketActivityExists(quill::Logger *logger,
                                               PGconn *pg,
                                               const std::string &ticket_id,
                                               const std::string &activity_id) {
  const char *params[] = {ticket_id.c_str(), activity_id.c_str()};
  PGresult *res = PQexecParams(pg,
                               R"SQL(
SELECT id::text
  FROM helpdesk.ticket_activity
 WHERE ticket_id = $1::bigint
   AND id = $2::bigint
)SQL",
                               2, nullptr, params, nullptr, nullptr, 0);

  if (PQresultStatus(res) != PGRES_TUPLES_OK) {
    LOGJ_DEBUG(logger, "[supervisor] ticket activity query error",
               PQresultErrorMessage(res));
    PQclear(res);
    return false;
  }

  const bool exists = PQntuples(res) == 1;
  PQclear(res);
  return exists;
}

[[nodiscard]] inline std::optional<std::string>
NextTicketActivityAttachmentId(quill::Logger *logger, PGconn *pg) {
  PGresult *res = PQexecParams(pg,
                               R"SQL(
SELECT nextval(pg_get_serial_sequence('helpdesk.ticket_activity_attachment', 'id'))::text
)SQL",
                               0, nullptr, nullptr, nullptr, nullptr, 0);
  if (PQresultStatus(res) != PGRES_TUPLES_OK || PQntuples(res) != 1) {
    LOGJ_DEBUG(logger, "[supervisor] next attachment id query error",
               PQresultErrorMessage(res));
    PQclear(res);
    return std::nullopt;
  }

  std::string attachment_id = PQgetvalue(res, 0, 0);
  PQclear(res);
  return attachment_id;
}

[[nodiscard]] inline std::optional<Json::Value> TicketActivityAttachmentCreate(
    quill::Logger *logger, PGconn *pg, const std::string &attachment_id,
    const std::string &activity_id, const std::string &file_path,
    const std::string &file_name, std::size_t file_size,
    const std::string &mime_type) {
  const std::string file_size_str = std::to_string(file_size);
  const char *params[] = {attachment_id.c_str(), activity_id.c_str(),
                          file_path.c_str(),     file_name.c_str(),
                          file_size_str.c_str(), mime_type.c_str()};
  PGresult *insert_res = PQexecParams(pg,
                                      R"SQL(
INSERT INTO helpdesk.ticket_activity_attachment
       (id, ticket_activity_id, file_path, file_name, file_size, mime_type)
VALUES ($1::bigint, $2::bigint, $3, $4, $5::bigint, $6)
RETURNING id::text, ticket_activity_id::text, file_path, file_name,
          file_size::text, mime_type, created_at::text
)SQL",
                                      6, nullptr, params, nullptr, nullptr, 0);

  if (PQresultStatus(insert_res) != PGRES_TUPLES_OK ||
      PQntuples(insert_res) != 1) {
    LOGJ_DEBUG(logger, "[supervisor] insert attachment error",
               PQresultErrorMessage(insert_res));
    PQclear(insert_res);
    return std::nullopt;
  }

  Json::Value attachment;
  attachment["id"] = PQgetvalue(insert_res, 0, 0);
  attachment["ticket_activity_id"] = PQgetvalue(insert_res, 0, 1);
  attachment["file_path"] = PQgetvalue(insert_res, 0, 2);
  attachment["file_name"] = PQgetvalue(insert_res, 0, 3);
  attachment["file_size"] = PQgetvalue(insert_res, 0, 4);
  attachment["mime_type"] = PQgetvalue(insert_res, 0, 5);
  attachment["created_at"] = PQgetvalue(insert_res, 0, 6);
  PQclear(insert_res);
  return attachment;
}

[[nodiscard]] inline std::optional<AttachmentFile>
FetchTicketActivityAttachmentFile(quill::Logger *logger, PGconn *pg,
                                  const std::string &ticket_id,
                                  const std::string &activity_id,
                                  const std::string &attachment_id) {
  const char *params[] = {ticket_id.c_str(), activity_id.c_str(),
                          attachment_id.c_str()};
  PGresult *res = PQexecParams(pg,
                               R"SQL(
SELECT taa.file_path,
       taa.file_name,
       taa.mime_type
  FROM helpdesk.ticket_activity_attachment taa
  JOIN helpdesk.ticket_activity ta
    ON ta.id = taa.ticket_activity_id
 WHERE ta.ticket_id = $1::bigint
   AND ta.id = $2::bigint
   AND taa.id = $3::bigint
)SQL",
                               3, nullptr, params, nullptr, nullptr, 0);

  if (PQresultStatus(res) != PGRES_TUPLES_OK || PQntuples(res) != 1) {
    LOGJ_DEBUG(logger, "[supervisor] attachment download query error",
               PQresultErrorMessage(res));
    PQclear(res);
    return std::nullopt;
  }

  AttachmentFile file{PQgetvalue(res, 0, 0), PQgetvalue(res, 0, 1),
                      PQgetvalue(res, 0, 2)};
  PQclear(res);
  return file;
}

} // namespace

namespace ticketeer::api::role {

void Supervisor::TicketList(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  auto *logger = quill::Frontend::get_logger("root");
  const auto auth_header = req->getHeader("Authorization");
  if (auth_header.empty() || auth_header.rfind("Bearer ", 0) != 0)
    return BadRequest(callback, "Missing Authorization");

  const std::string token = auth_header.substr(7);

  PGconn *pg = ConnectDB();
  if (PQstatus(pg) != CONNECTION_OK) {
    PQfinish(pg);
    return BadRequest(callback, "Database unavailable",
                      drogon::k503ServiceUnavailable);
  }

  auto profile_id = FetchProfileId(logger, pg, token);
  if (!profile_id) {
    PQfinish(pg);
    return BadRequest(callback, "Forbidden: insufficient permissions",
                      drogon::k403Forbidden);
  }

  const std::string search = req->getParameter("s");
  Json::Value tickets = ticketeer::api::role::common::FetchSupervisorTicketList(
      logger, pg,
      search.empty() ? std::nullopt : std::optional<std::string>{search});
  PQfinish(pg);

  callback(drogon::HttpResponse::newHttpJsonResponse(tickets));
}

void Supervisor::TicketCreate(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  auto *logger = quill::Frontend::get_logger("root");
  const auto auth_header = req->getHeader("Authorization");
  if (auth_header.empty() || auth_header.rfind("Bearer ", 0) != 0)
    return BadRequest(callback, "Missing Authorization");

  const std::string token = auth_header.substr(7);

  auto body = req->getJsonObject();
  if (!body)
    return BadRequest(callback, "Invalid JSON body");

  const auto &request_type_id = (*body)["request_type_id"];
  const auto &department_id = (*body)["department_id"];
  const auto &priority_id = (*body)["priority_id"];
  const auto &description = (*body)["description"];
  const auto &due_date = (*body)["due_date"];

  if (!request_type_id.isString() || !department_id.isString() ||
      !priority_id.isString() || !description.isString())
    return BadRequest(callback,
                      "Missing or invalid required fields: request_type_id, "
                      "department_id, priority_id, description");

  PGconn *pg = ConnectDB();
  if (PQstatus(pg) != CONNECTION_OK) {
    PQfinish(pg);
    return BadRequest(callback, "Database unavailable",
                      drogon::k503ServiceUnavailable);
  }

  auto profile_id = FetchProfileId(logger, pg, token);
  if (!profile_id) {
    PQfinish(pg);
    return BadRequest(callback, "Invalid or expired token",
                      drogon::k401Unauthorized);
  }

  auto default_status_id = FetchDefaultStatusId(logger, pg);
  if (!default_status_id) {
    PQfinish(pg);
    return BadRequest(callback, "Failed to get default status");
  }

  auto default_assigned_to_id = FetchDefaultAssignedToId(logger, pg);
  if (!default_assigned_to_id) {
    PQfinish(pg);
    return BadRequest(callback, "Failed to get default assigned to");
  }

  auto ticket = CreateTicket(logger, pg, *profile_id, request_type_id,
                             department_id, priority_id, *default_status_id,
                             *default_assigned_to_id, description, due_date);
  if (!ticket) {
    PQfinish(pg);
    return BadRequest(callback, "Failed to create ticket");
  }

  PQfinish(pg);

  callback(drogon::HttpResponse::newHttpJsonResponse(*ticket));
}

void Supervisor::TicketDetails(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback,
    const std::string &ticket_id) {
  auto *logger = quill::Frontend::get_logger("root");
  const auto auth_header = req->getHeader("Authorization");
  if (auth_header.empty() || auth_header.rfind("Bearer ", 0) != 0)
    return BadRequest(callback, "Missing Authorization");

  PGconn *pg = ConnectDB();
  if (PQstatus(pg) != CONNECTION_OK) {
    PQfinish(pg);
    return BadRequest(callback, "Database unavailable",
                      drogon::k503ServiceUnavailable);
  }

  const std::string token = auth_header.substr(7);
  auto profile_id = FetchProfileId(logger, pg, token);
  if (!profile_id) {
    PQfinish(pg);
    return BadRequest(callback, "Forbidden: insufficient permissions",
                      drogon::k403Forbidden);
  }

  auto ticket = FetchTicketDetails(logger, pg, ticket_id);
  if (ticket.status == TicketLookupStatus::QueryFailed) {
    PQfinish(pg);
    return BadRequest(callback, "Database query failed");
  }

  if (ticket.status == TicketLookupStatus::NotFound) {
    PQfinish(pg);
    return BadRequest(callback, "Ticket not found", drogon::k404NotFound);
  }

  auto activities = FetchTicketActivityList(logger, pg, ticket_id);
  if (activities.status == TicketLookupStatus::QueryFailed) {
    PQfinish(pg);
    return BadRequest(callback, "Database query failed");
  }

  if (activities.status == TicketLookupStatus::NotFound) {
    PQfinish(pg);
    return BadRequest(callback, "Ticket not found", drogon::k404NotFound);
  }

  ticket.ticket["activities"] = activities.ticket;

  PQfinish(pg);
  callback(drogon::HttpResponse::newHttpJsonResponse(ticket.ticket));
}

void Supervisor::TicketActivityList(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback,
    const std::string &ticket_id) {
  auto *logger = quill::Frontend::get_logger("root");
  const auto auth_header = req->getHeader("Authorization");
  if (auth_header.empty() || auth_header.rfind("Bearer ", 0) != 0)
    return BadRequest(callback, "Missing Authorization");

  PGconn *pg = ConnectDB();
  if (PQstatus(pg) != CONNECTION_OK) {
    PQfinish(pg);
    return BadRequest(callback, "Database unavailable",
                      drogon::k503ServiceUnavailable);
  }

  const std::string token = auth_header.substr(7);
  auto profile_id = FetchProfileId(logger, pg, token);
  if (!profile_id) {
    PQfinish(pg);
    return BadRequest(callback, "Forbidden: insufficient permissions",
                      drogon::k403Forbidden);
  }

  auto activities = FetchTicketActivityList(logger, pg, ticket_id);
  if (activities.status == TicketLookupStatus::QueryFailed) {
    PQfinish(pg);
    return BadRequest(callback, "Database query failed");
  }

  if (activities.status == TicketLookupStatus::NotFound) {
    PQfinish(pg);
    return BadRequest(callback, "Ticket not found", drogon::k404NotFound);
  }

  PQfinish(pg);
  callback(drogon::HttpResponse::newHttpJsonResponse(activities.ticket));
}

void Supervisor::TicketActivityCreateMessage(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback,
    const std::string &ticket_id) {
  auto *logger = quill::Frontend::get_logger("root");
  const auto auth_header = req->getHeader("Authorization");
  if (auth_header.empty() || auth_header.rfind("Bearer ", 0) != 0)
    return BadRequest(callback, "Missing Authorization");

  const std::string token = auth_header.substr(7);

  auto body = req->getJsonObject();
  if (!body)
    return BadRequest(callback, "Invalid JSON body");

  const auto &message = (*body)["message"];
  if (!message.isString())
    return BadRequest(callback, "Missing or invalid field: message");

  PGconn *pg = ConnectDB();
  if (PQstatus(pg) != CONNECTION_OK) {
    PQfinish(pg);
    return BadRequest(callback, "Database unavailable",
                      drogon::k503ServiceUnavailable);
  }

  auto profile_id = FetchProfileId(logger, pg, token);
  if (!profile_id) {
    PQfinish(pg);
    return BadRequest(callback, "Invalid or expired token",
                      drogon::k401Unauthorized);
  }

  const char *ticket_params[] = {ticket_id.c_str()};
  PGresult *ticket_res =
      PQexecParams(pg,
                   R"SQL(
SELECT id::text
  FROM helpdesk.ticket
 WHERE id = $1::bigint
      )SQL",
                   1, nullptr, ticket_params, nullptr, nullptr, 0);

  if (PQresultStatus(ticket_res) != PGRES_TUPLES_OK) {
    LOGJ_DEBUG(logger, "[supervisor] ticket owner query error",
               PQresultErrorMessage(ticket_res));
    PQclear(ticket_res);
    PQfinish(pg);
    return BadRequest(callback, "Database query failed");
  }

  if (PQntuples(ticket_res) != 1) {
    PQclear(ticket_res);
    PQfinish(pg);
    return BadRequest(callback, "Ticket not found", drogon::k404NotFound);
  }
  PQclear(ticket_res);

  // Create activity
  auto activity = CreateTicketActivityMessage(logger, pg, ticket_id,
                                              *profile_id, message.asString());
  if (!activity) {
    PQfinish(pg);
    return BadRequest(callback, "Failed to create activity");
  }

  PQfinish(pg);

  callback(drogon::HttpResponse::newHttpJsonResponse(*activity));
}

void Supervisor::TicketActivityAttachmentCreate(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback,
    const std::string &ticket_id, const std::string &activity_id) {
  auto *logger = quill::Frontend::get_logger("root");
  const auto auth_header = req->getHeader("Authorization");
  if (auth_header.empty() || auth_header.rfind("Bearer ", 0) != 0)
    return BadRequest(callback, "Missing Authorization");

  if (req->contentType() != drogon::CT_MULTIPART_FORM_DATA)
    return BadRequest(callback, "Content-Type must be multipart/form-data");

  drogon::MultiPartParser parser;
  if (parser.parse(req) != 0)
    return BadRequest(callback, "Invalid multipart form-data");

  const auto &files = parser.getFiles();
  if (files.empty())
    return BadRequest(callback, "Missing file");

  const auto &file = files.front();
  const std::string file_name =
      std::filesystem::path(file.getFileName()).filename().string();
  if (file_name.empty())
    return BadRequest(callback, "Missing file name");

  const auto file_size = file.fileLength();
  if (file_size > MaxAttachmentSize)
    return BadRequest(callback, "File exceeds 5MB limit",
                      drogon::k413RequestEntityTooLarge);

  const std::string token = auth_header.substr(7);

  PGconn *pg = ConnectDB();
  if (PQstatus(pg) != CONNECTION_OK) {
    PQfinish(pg);
    return BadRequest(callback, "Database unavailable",
                      drogon::k503ServiceUnavailable);
  }

  auto profile_id = FetchProfileId(logger, pg, token);
  if (!profile_id) {
    PQfinish(pg);
    return BadRequest(callback, "Forbidden: insufficient permissions",
                      drogon::k403Forbidden);
  }

  if (!TicketActivityExists(logger, pg, ticket_id, activity_id)) {
    PQfinish(pg);
    return BadRequest(callback, "Ticket activity not found",
                      drogon::k404NotFound);
  }

  auto attachment_id = NextTicketActivityAttachmentId(logger, pg);
  if (!attachment_id) {
    PQfinish(pg);
    return BadRequest(callback, "Failed to create attachment");
  }

  const std::filesystem::path relative_path =
      std::filesystem::path("role") / ("ticket_id=" + ticket_id) /
      ("activity_id=" + activity_id) / ("attachment_id=" + *attachment_id);
  const std::filesystem::path absolute_path =
      std::filesystem::current_path() /
      ticketeer::core::conf::settings.UPLOAD_DIR / relative_path;

  if (!WriteFile(absolute_path / file_name, file.fileContent())) {
    PQfinish(pg);
    return BadRequest(callback, "Failed to save attachment");
  }

  const std::string file_path = relative_path.generic_string();
  auto attachment = ::TicketActivityAttachmentCreate(
      logger, pg, *attachment_id, activity_id, file_path, file_name, file_size,
      InferMimeType(file_name));
  if (!attachment) {
    std::error_code ec;
    std::filesystem::remove(absolute_path / file_name, ec);
    PQfinish(pg);
    return BadRequest(callback, "Failed to create attachment");
  }

  PQfinish(pg);
  callback(drogon::HttpResponse::newHttpJsonResponse(*attachment));
}

void Supervisor::TicketActivityAttachmentDownload(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback,
    const std::string &ticket_id, const std::string &activity_id,
    const std::string &attachment_id) {
  auto *logger = quill::Frontend::get_logger("root");
  const auto auth_header = req->getHeader("Authorization");
  if (auth_header.empty() || auth_header.rfind("Bearer ", 0) != 0)
    return BadRequest(callback, "Missing Authorization");

  PGconn *pg = ConnectDB();
  if (PQstatus(pg) != CONNECTION_OK) {
    PQfinish(pg);
    return BadRequest(callback, "Database unavailable",
                      drogon::k503ServiceUnavailable);
  }

  const std::string token = auth_header.substr(7);
  auto profile_id = FetchProfileId(logger, pg, token);
  if (!profile_id) {
    PQfinish(pg);
    return BadRequest(callback, "Forbidden: insufficient permissions",
                      drogon::k403Forbidden);
  }

  auto file = FetchTicketActivityAttachmentFile(logger, pg, ticket_id,
                                                activity_id, attachment_id);
  PQfinish(pg);

  if (!file)
    return BadRequest(callback, "Attachment not found", drogon::k404NotFound);

  const auto absolute_path =
      AttachmentAbsolutePath(file->file_path, file->file_name);
  if (!std::filesystem::is_regular_file(absolute_path))
    return BadRequest(callback, "Attachment file not found",
                      drogon::k404NotFound);

  callback(drogon::HttpResponse::newFileResponse(
      absolute_path.string(), file->file_name, drogon::CT_CUSTOM,
      file->mime_type, req));
}

} // namespace ticketeer::api::role
