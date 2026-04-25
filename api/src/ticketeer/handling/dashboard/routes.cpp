#include "routes.hpp"

#include <string>

#include <json/json.h>
#include <libpq-fe.h>
#include <quill/Frontend.h>
#include <quill/LogMacros.h>

#include "ticketeer/core/conf.hpp"

namespace {

using Callback = std::function<void(const drogon::HttpResponsePtr &)>;

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

[[nodiscard]] inline Json::Value FetchProfile(quill::Logger *logger, PGconn *pg,
                                              const std::string &token) {
  const char *params[] = {token.c_str()};
  PGresult *res = PQexecParams(pg,
                               R"SQL(
WITH authenticated_user AS (
  SELECT user_id
    FROM auth.session
   WHERE token = $1
     AND expires_at > now()
), inserted_profile AS (
  INSERT INTO helpdesk.profile (user_id, department_id, role, name)
  SELECT au.user_id,
         hs.default_department_id,
         'requester'::helpdesk.role,
         u.username
    FROM authenticated_user au
    JOIN auth."user" u
      ON u.id = au.user_id
   CROSS JOIN helpdesk.setting hs
   WHERE hs.name = 'default'
  ON CONFLICT (user_id) DO NOTHING
  RETURNING id,
            user_id,
            department_id,
            role::text,
            name
), selected_profile AS (
  SELECT ip.id,
         ip.user_id,
         u.username,
         ip.department_id,
         ip.role::text,
         ip.name
    FROM inserted_profile ip
    JOIN auth."user" u
      ON u.id = ip.user_id
  UNION ALL
  SELECT hp.id,
         hp.user_id,
         u.username,
         hp.department_id,
         hp.role::text,
         hp.name
    FROM helpdesk.profile hp
    JOIN authenticated_user au
      ON au.user_id = hp.user_id
    JOIN auth."user" u
      ON u.id = hp.user_id
   LIMIT 1
)
SELECT 'profile',
       id::text,
       user_id::text,
       username,
       department_id::text,
       role::text,
       name
  FROM selected_profile
 LIMIT 1
)SQL",
                               1, nullptr, params, nullptr, nullptr, 0);

  const auto status = PQresultStatus(res);
  if (status != PGRES_TUPLES_OK) {
    LOGJ_DEBUG(logger, "[dashboard] profile query error",
               PQresultErrorMessage(res));
    PQclear(res);
    return Json::Value{};
  }

  if (PQntuples(res) != 1) {
    LOGJ_DEBUG(logger, "[dashboard] profile not found");
    PQclear(res);
    return Json::Value{};
  }

  Json::Value profile;
  profile["id"] = PQgetvalue(res, 0, 1);
  profile["department_id"] = PQgetvalue(res, 0, 4);
  profile["role"] = PQgetvalue(res, 0, 5);
  profile["name"] = PQgetvalue(res, 0, 6);
  profile["user"]["id"] = PQgetvalue(res, 0, 2);
  profile["user"]["username"] = PQgetvalue(res, 0, 3);

  PQclear(res);
  return profile;
}

[[nodiscard]] inline Json::Value FetchDepartments(quill::Logger *logger,
                                                  PGconn *pg) {
  Json::Value result = Json::arrayValue;
  PGresult *departments =
      PQexecParams(pg,
                   R"SQL(
SELECT id::text,
       name,
       description
  FROM helpdesk.department
 ORDER BY name
)SQL",
                   0, nullptr, nullptr, nullptr, nullptr, 0);

  if (PQresultStatus(departments) != PGRES_TUPLES_OK) {
    LOGJ_DEBUG(logger, "[dashboard] department query error",
               PQresultErrorMessage(departments));
    PQclear(departments);
    return result;
  }

  for (int i = 0; i < PQntuples(departments); ++i) {
    Json::Value department;
    department["id"] = PQgetvalue(departments, i, 0);
    department["name"] = PQgetvalue(departments, i, 1);
    department["description"] = PQgetvalue(departments, i, 2);
    result.append(department);
  }
  PQclear(departments);

  return result;
}

[[nodiscard]] inline Json::Value FetchTicketStatuses(quill::Logger *logger,
                                                     PGconn *pg) {
  Json::Value result = Json::arrayValue;
  PGresult *statuses = PQexecParams(pg,
                                    R"SQL(
SELECT id::text,
       name,
       display_name,
       trait::text
  FROM helpdesk.ticket_status
 ORDER BY id
)SQL",
                                    0, nullptr, nullptr, nullptr, nullptr, 0);

  if (PQresultStatus(statuses) != PGRES_TUPLES_OK) {
    LOGJ_DEBUG(logger, "[dashboard] ticket status query error",
               PQresultErrorMessage(statuses));
    PQclear(statuses);
    return result;
  }

  for (int i = 0; i < PQntuples(statuses); ++i) {
    Json::Value status;
    status["id"] = PQgetvalue(statuses, i, 0);
    status["name"] = PQgetvalue(statuses, i, 1);
    status["display_name"] = PQgetvalue(statuses, i, 2);
    status["trait"] = PQgetvalue(statuses, i, 3);
    result.append(status);
  }
  PQclear(statuses);

  return result;
}

[[nodiscard]] inline Json::Value FetchDefaults(quill::Logger *logger,
                                               PGconn *pg) {
  PGresult *res = PQexecParams(pg,
                               R"SQL(
SELECT default_status_id::text
  FROM helpdesk.setting
 WHERE name = 'default'
)SQL",
                               0, nullptr, nullptr, nullptr, nullptr, 0);

  if (PQresultStatus(res) != PGRES_TUPLES_OK || PQntuples(res) != 1) {
    LOGJ_DEBUG(logger, "[dashboard] defaults query error",
               PQresultErrorMessage(res));
    PQclear(res);
    return Json::Value{};
  }

  Json::Value defaults;
  defaults["status_id"] = PQgetvalue(res, 0, 0);
  PQclear(res);
  return defaults;
}

[[nodiscard]] inline Json::Value FetchRequestTypes(quill::Logger *logger,
                                                   PGconn *pg) {
  Json::Value result = Json::arrayValue;
  PGresult *types = PQexecParams(pg,
                                 R"SQL(
SELECT rt.id::text,
       rt.name,
       rt.description,
       c.name AS category,
       p.display_name AS default_priority
  FROM helpdesk.request_type rt
  JOIN helpdesk.request_category c
    ON c.id = rt.category_id
  JOIN helpdesk.priority p
    ON p.id = rt.default_priority_id
)SQL"
                                 " ORDER BY rt.name",
                                 0, nullptr, nullptr, nullptr, nullptr, 0);

  if (PQresultStatus(types) != PGRES_TUPLES_OK) {
    LOGJ_DEBUG(logger, "[dashboard] request type query error",
               PQresultErrorMessage(types));
    PQclear(types);
    return result;
  }

  for (int i = 0; i < PQntuples(types); ++i) {
    Json::Value type;
    type["id"] = PQgetvalue(types, i, 0);
    type["name"] = PQgetvalue(types, i, 1);
    type["description"] = PQgetvalue(types, i, 2);
    type["category"] = PQgetvalue(types, i, 3);
    type["default_priority"] = PQgetvalue(types, i, 4);
    result.append(type);
  }
  PQclear(types);
  return result;
}

[[nodiscard]] inline Json::Value FetchPriorities(quill::Logger *logger,
                                                 PGconn *pg) {
  Json::Value result = Json::arrayValue;
  PGresult *priorities = PQexecParams(pg,
                                      R"SQL(
SELECT id::text,
       name,
       display_name
  FROM helpdesk.priority
 ORDER BY id
)SQL",
                                      0, nullptr, nullptr, nullptr, nullptr, 0);

  if (PQresultStatus(priorities) != PGRES_TUPLES_OK) {
    LOGJ_DEBUG(logger, "[dashboard] priority query error",
               PQresultErrorMessage(priorities));
    PQclear(priorities);
    return result;
  }

  for (int i = 0; i < PQntuples(priorities); ++i) {
    Json::Value priority;
    priority["id"] = PQgetvalue(priorities, i, 0);
    priority["name"] = PQgetvalue(priorities, i, 1);
    priority["display_name"] = PQgetvalue(priorities, i, 2);
    result.append(priority);
  }
  PQclear(priorities);
  return result;
}

} // namespace

namespace ticketeer::api {

void Dashboard::Landing(
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

  Json::Value profile = FetchProfile(logger, pg, token);
  if (profile.isNull()) {
    PQfinish(pg);
    return BadRequest(callback, "Profile fetch failed",
                      drogon::k500InternalServerError);
  }

  Json::Value result;
  result["profile"] = profile;
  result["departments"] = FetchDepartments(logger, pg);
  result["ticket_statuses"] = FetchTicketStatuses(logger, pg);
  result["request_types"] = FetchRequestTypes(logger, pg);
  result["priorities"] = FetchPriorities(logger, pg);
  result["defaults"] = FetchDefaults(logger, pg);

  PQfinish(pg);
  callback(drogon::HttpResponse::newHttpJsonResponse(result));
}

} // namespace ticketeer::api
