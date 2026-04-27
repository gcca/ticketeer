#include "routes.hpp"

#include <cstring>
#include <optional>
#include <string>
#include <vector>

#include <json/json.h>
#include <libpq-fe.h>
#include <quill/Frontend.h>
#include <quill/LogMacros.h>

#include "ticketeer/core/conf.hpp"

namespace {

using Callback = std::function<void(const drogon::HttpResponsePtr &)>;

enum class TicketLookupStatus { Found, NotFound, QueryFailed };

struct TicketLookup {
  TicketLookupStatus status;
  Json::Value ticket;
};

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

[[nodiscard]] inline Json::Value FetchTicketList(quill::Logger *logger,
                                                 PGconn *pg) {
  Json::Value result = Json::arrayValue;
  PGresult *tickets = PQexecParams(pg,
                                   R"SQL(
SELECT t.id::text,
       t.description,
       t.status_id::text,
       t.created_at::text
  FROM helpdesk.ticket t
 ORDER BY t.created_at DESC
)SQL",
                                   0, nullptr, nullptr, nullptr, nullptr, 0);

  if (PQresultStatus(tickets) != PGRES_TUPLES_OK) {
    LOGJ_DEBUG(logger, "[supervisor] ticket list query error",
               PQresultErrorMessage(tickets));
    PQclear(tickets);
    return result;
  }

  for (int i = 0; i < PQntuples(tickets); ++i) {
    Json::Value ticket;
    ticket["id"] = PQgetvalue(tickets, i, 0);
    ticket["description"] = PQgetvalue(tickets, i, 1);
    ticket["status_id"] = PQgetvalue(tickets, i, 2);
    ticket["created_at"] = PQgetvalue(tickets, i, 3);
    ticket["activities"] = Json::arrayValue;
    result.append(ticket);
  }
  PQclear(tickets);

  return result;
}

inline void AppendActivityPreviews(quill::Logger *logger, PGconn *pg,
                                   Json::Value &tickets) {
  for (Json::ArrayIndex i = 0; i < tickets.size(); ++i) {
    std::string ticket_id = tickets[i]["id"].asString();
    const char *params[] = {ticket_id.c_str()};
    PGresult *activities =
        PQexecParams(pg,
                     R"SQL(
SELECT body,
       created_at::text
  FROM helpdesk.ticket_activity
 WHERE ticket_id = $1::bigint
 ORDER BY created_at DESC
)SQL",
                     1, nullptr, params, nullptr, nullptr, 0);

    if (PQresultStatus(activities) != PGRES_TUPLES_OK) {
      LOGJ_DEBUG(logger, "[supervisor] ticket activity preview query error",
                 PQresultErrorMessage(activities));
      PQclear(activities);
      continue;
    }

    for (int j = 0; j < PQntuples(activities); ++j) {
      Json::Value activity;
      activity["body"] = PQgetvalue(activities, j, 0);
      activity["created_at"] = PQgetvalue(activities, j, 1);
      tickets[i]["activities"].append(activity);
    }
    PQclear(activities);
  }
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
    Json::Value activity;
    activity["id"] = PQgetvalue(activities, i, 0);
    activity["kind"] = PQgetvalue(activities, i, 1);
    activity["body"] = PQgetvalue(activities, i, 2);
    activity["created_at"] = PQgetvalue(activities, i, 3);
    activity["profile_name"] = PQgetvalue(activities, i, 4);
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

  Json::Value tickets = FetchTicketList(logger, pg);
  AppendActivityPreviews(logger, pg, tickets);
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

} // namespace ticketeer::api::role
