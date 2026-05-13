#include "routes.hpp"

#include <map>
#include <string>
#include <vector>

#include <drogon/DrTemplateBase.h>
#include <json/json.h>
#include <quill/Frontend.h>
#include <quill/LogMacros.h>
#include <sqlite3.h>

#include "ticketeer/core/conf.hpp"

namespace {

using Callback = std::function<void(const drogon::HttpResponsePtr &)>;
using Row      = std::map<std::string, std::string>;
using Rows     = std::vector<Row>;

inline void BadRequest(const Callback &callback, const char *msg,
                       drogon::HttpStatusCode code = drogon::k400BadRequest) {
  auto resp = drogon::HttpResponse::newHttpResponse();
  resp->setStatusCode(code);
  resp->setBody(msg);
  callback(resp);
}

[[nodiscard]] inline sqlite3 *ConnectDB() {
  std::string path = ticketeer::core::conf::settings.DB_URL;
  if (path.starts_with("sqlite:")) path = path.substr(7);
  sqlite3 *db = nullptr;
  if (sqlite3_open(path.c_str(), &db) != SQLITE_OK) {
    sqlite3_close(db);
    return nullptr;
  }
  return db;
}

struct Profile {
  std::string id;
  std::string username;
  std::string name;
};

[[nodiscard]] inline std::optional<Profile>
FetchProfile(quill::Logger *logger, sqlite3 *db, const std::string &token) {
  sqlite3_stmt *stmt = nullptr;
  const char *sql = R"SQL(
SELECT p.id, u.username, p.name
  FROM helpdesk_profile p
  JOIN auth_session s ON s.user_id = p.user_id
  JOIN auth_user u    ON u.id = p.user_id
 WHERE s.token = ?
   AND s.expires_at > datetime('now')
   AND p.role = 'requester'
 LIMIT 1
)SQL";

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    LOGJ_DEBUG(logger, "[requester] profile query error", sqlite3_errmsg(db));
    return std::nullopt;
  }
  sqlite3_bind_text(stmt, 1, token.c_str(), -1, SQLITE_STATIC);

  std::optional<Profile> profile;
  if (sqlite3_step(stmt) == SQLITE_ROW)
    profile = Profile{
        reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0)),
        reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1)),
        reinterpret_cast<const char *>(sqlite3_column_text(stmt, 2))};
  else
    LOGJ_DEBUG(logger, "[requester] profile not found");

  sqlite3_finalize(stmt);
  return profile;
}

[[nodiscard]] inline Rows
FetchTickets(quill::Logger *logger, sqlite3 *db,
             const std::string &profile_id, const std::string &search) {
  sqlite3_stmt *stmt = nullptr;
  std::string sql = R"SQL(
SELECT t.id, t.description, ts.display_name, p.display_name, rt.name, t.created_at
  FROM helpdesk_ticket t
  JOIN helpdesk_ticket_status ts ON ts.id = t.status_id
  JOIN helpdesk_priority p       ON p.id  = t.priority_id
  JOIN helpdesk_request_type rt  ON rt.id = t.request_type_id
 WHERE t.requester_id = ?
)SQL";
  if (!search.empty()) sql += " AND t.description LIKE ?";
  sql += " ORDER BY t.created_at DESC";

  if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
    LOGJ_DEBUG(logger, "[requester] ticket list query error", sqlite3_errmsg(db));
    return {};
  }
  sqlite3_bind_text(stmt, 1, profile_id.c_str(), -1, SQLITE_STATIC);
  std::string like_search;
  if (!search.empty()) {
    like_search = "%" + search + "%";
    sqlite3_bind_text(stmt, 2, like_search.c_str(), -1, SQLITE_STATIC);
  }

  Rows tickets;
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    auto col = [&](int i) -> std::string {
      const auto *v = sqlite3_column_text(stmt, i);
      return v ? reinterpret_cast<const char *>(v) : "";
    };
    tickets.push_back({{"id", col(0)}, {"description", col(1)},
                       {"status_name", col(2)}, {"priority_name", col(3)},
                       {"request_type_name", col(4)}, {"created_at", col(5)}});
  }
  sqlite3_finalize(stmt);
  return tickets;
}

[[nodiscard]] inline std::optional<Row>
FetchTicketDetails(quill::Logger *logger, sqlite3 *db,
                   const std::string &ticket_id,
                   const std::string &profile_id) {
  sqlite3_stmt *stmt = nullptr;
  const char *sql = R"SQL(
SELECT t.id, t.description, ts.display_name, p.display_name,
       rt.name, d.name, t.created_at, t.due_date,
       COALESCE(ap.name, '') AS assigned_to_name
  FROM helpdesk_ticket t
  JOIN helpdesk_ticket_status ts ON ts.id = t.status_id
  JOIN helpdesk_priority p       ON p.id  = t.priority_id
  JOIN helpdesk_request_type rt  ON rt.id = t.request_type_id
  JOIN helpdesk_department d     ON d.id  = t.department_id
  LEFT JOIN helpdesk_profile ap  ON ap.id = t.assigned_to_id
 WHERE t.id = ?
   AND t.requester_id = ?
 LIMIT 1
)SQL";

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    LOGJ_DEBUG(logger, "[requester] ticket details query error", sqlite3_errmsg(db));
    return std::nullopt;
  }
  sqlite3_bind_text(stmt, 1, ticket_id.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 2, profile_id.c_str(), -1, SQLITE_STATIC);

  std::optional<Row> ticket;
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    auto col = [&](int i) -> std::string {
      const auto *v = sqlite3_column_text(stmt, i);
      return v ? reinterpret_cast<const char *>(v) : "";
    };
    ticket = Row{{"id", col(0)}, {"description", col(1)},
                 {"status_name", col(2)}, {"priority_name", col(3)},
                 {"request_type_name", col(4)}, {"department_name", col(5)},
                 {"created_at", col(6)}, {"due_date", col(7)},
                 {"assigned_to_name", col(8)}};
  }
  sqlite3_finalize(stmt);
  return ticket;
}

[[nodiscard]] inline Rows
FetchActivities(quill::Logger *logger, sqlite3 *db,
                const std::string &ticket_id) {
  sqlite3_stmt *stmt = nullptr;
  const char *sql = R"SQL(
SELECT ta.id, ta.body, ta.created_at, p.name
  FROM helpdesk_ticket_activity ta
  JOIN helpdesk_profile p ON p.id = ta.profile_id
 WHERE ta.ticket_id = ?
 ORDER BY ta.created_at ASC, ta.id ASC
)SQL";

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    LOGJ_DEBUG(logger, "[requester] activities query error", sqlite3_errmsg(db));
    return {};
  }
  sqlite3_bind_text(stmt, 1, ticket_id.c_str(), -1, SQLITE_STATIC);

  Rows activities;
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    auto col = [&](int i) -> std::string {
      const auto *v = sqlite3_column_text(stmt, i);
      return v ? reinterpret_cast<const char *>(v) : "";
    };
    activities.push_back({{"id", col(0)}, {"body", col(1)},
                          {"created_at", col(2)}, {"profile_name", col(3)}});
  }
  sqlite3_finalize(stmt);
  return activities;
}

[[nodiscard]] inline std::optional<std::string>
FetchDefaultStatusId(quill::Logger *logger, sqlite3 *db) {
  sqlite3_stmt *stmt = nullptr;
  if (sqlite3_prepare_v2(db,
          "SELECT default_status_id FROM helpdesk_setting WHERE name = 'default'",
          -1, &stmt, nullptr) != SQLITE_OK) {
    LOGJ_DEBUG(logger, "[requester] default status query error", sqlite3_errmsg(db));
    return std::nullopt;
  }
  std::optional<std::string> id;
  if (sqlite3_step(stmt) == SQLITE_ROW)
    id = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
  sqlite3_finalize(stmt);
  return id;
}

[[nodiscard]] inline std::optional<std::string>
FetchDefaultAssignedToId(quill::Logger *logger, sqlite3 *db) {
  sqlite3_stmt *stmt = nullptr;
  if (sqlite3_prepare_v2(db,
          "SELECT default_assigned_to_id FROM helpdesk_setting WHERE name = 'default'",
          -1, &stmt, nullptr) != SQLITE_OK) {
    LOGJ_DEBUG(logger, "[requester] default assigned_to query error", sqlite3_errmsg(db));
    return std::nullopt;
  }
  std::optional<std::string> id;
  if (sqlite3_step(stmt) == SQLITE_ROW)
    id = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
  sqlite3_finalize(stmt);
  return id;
}

} // namespace

namespace ticketeer {

void Requester::Home(const drogon::HttpRequestPtr &req,
                         Callback &&callback) {
  auto *logger = quill::Frontend::get_logger("root");
  const auto token = req->getCookie("token");

  sqlite3 *db = ConnectDB();
  if (!db)
    return BadRequest(callback, "Database unavailable",
                      drogon::k503ServiceUnavailable);

  const auto profile = FetchProfile(logger, db, token);
  sqlite3_close(db);

  if (!profile)
    return callback(drogon::HttpResponse::newRedirectionResponse("/ticketeer/auth/signin"));

  drogon::HttpViewData data;
  data.insert("username", profile->username);
  data.insert("name", profile->name);
  callback(drogon::HttpResponse::newHttpViewResponse("home", data));
}

void Requester::TicketList(const drogon::HttpRequestPtr &req,
                           Callback &&callback) {
  auto *logger = quill::Frontend::get_logger("root");
  const auto token = req->getCookie("token");

  sqlite3 *db = ConnectDB();
  if (!db)
    return BadRequest(callback, "Database unavailable",
                      drogon::k503ServiceUnavailable);

  const auto profile = FetchProfile(logger, db, token);
  if (!profile) {
    sqlite3_close(db);
    return callback(drogon::HttpResponse::newRedirectionResponse("/ticketeer/auth/signin"));
  }

  const auto search = req->getParameter("s");
  auto tickets = FetchTickets(logger, db, profile->id, search);
  sqlite3_close(db);

  drogon::HttpViewData data;
  data.insert("tickets", tickets);
  data.insert("search", search);
  callback(drogon::HttpResponse::newHttpViewResponse("list", data));
}

void Requester::TicketCreate(const drogon::HttpRequestPtr &req,
                             Callback &&callback) {
  auto *logger = quill::Frontend::get_logger("root");
  const auto token = req->getCookie("token");

  const auto body = req->getJsonObject();
  if (!body)
    return BadRequest(callback, "Invalid JSON body");

  const auto &request_type_id = (*body)["request_type_id"];
  const auto &department_id   = (*body)["department_id"];
  const auto &priority_id     = (*body)["priority_id"];
  const auto &description     = (*body)["description"];
  const auto &due_date        = (*body)["due_date"];

  if (!request_type_id.isString() || !department_id.isString() ||
      !priority_id.isString() || !description.isString())
    return BadRequest(callback,
                      "Missing or invalid required fields: request_type_id, "
                      "department_id, priority_id, description");

  sqlite3 *db = ConnectDB();
  if (!db)
    return BadRequest(callback, "Database unavailable",
                      drogon::k503ServiceUnavailable);

  const auto profile = FetchProfile(logger, db, token);
  if (!profile) {
    sqlite3_close(db);
    return BadRequest(callback, "Invalid or expired token", drogon::k401Unauthorized);
  }

  const auto default_status_id = FetchDefaultStatusId(logger, db);
  if (!default_status_id) {
    sqlite3_close(db);
    return BadRequest(callback, "Failed to get default status");
  }

  const auto default_assigned_to_id = FetchDefaultAssignedToId(logger, db);
  if (!default_assigned_to_id) {
    sqlite3_close(db);
    return BadRequest(callback, "Failed to get default assigned to");
  }

  sqlite3_stmt *stmt = nullptr;
  std::string sql =
      "INSERT INTO helpdesk_ticket "
      "(request_type_id, requester_id, department_id, priority_id, "
      "status_id, assigned_to_id, description";
  const std::string due = due_date.isString() ? due_date.asString() : "";
  if (!due.empty()) sql += ", due_date";
  sql += ") VALUES (?, ?, ?, ?, ?, ?, ?";
  if (!due.empty()) sql += ", ?";
  sql += ")";

  if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
    LOGJ_DEBUG(logger, "[requester] insert ticket error", sqlite3_errmsg(db));
    sqlite3_close(db);
    return BadRequest(callback, "Failed to create ticket");
  }

  const auto rt  = request_type_id.asString();
  const auto dep = department_id.asString();
  const auto pri = priority_id.asString();
  const auto des = description.asString();
  sqlite3_bind_text(stmt, 1, rt.c_str(),                          -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 2, profile->id.c_str(),                 -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 3, dep.c_str(),                         -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 4, pri.c_str(),                         -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 5, default_status_id->c_str(),          -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 6, default_assigned_to_id->c_str(),     -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 7, des.c_str(),                         -1, SQLITE_STATIC);
  if (!due.empty())
    sqlite3_bind_text(stmt, 8, due.c_str(), -1, SQLITE_STATIC);

  if (sqlite3_step(stmt) != SQLITE_DONE) {
    LOGJ_DEBUG(logger, "[requester] insert ticket step error", sqlite3_errmsg(db));
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return BadRequest(callback, "Failed to create ticket");
  }
  sqlite3_finalize(stmt);

  auto tickets = FetchTickets(logger, db, profile->id, "");
  sqlite3_close(db);

  drogon::HttpViewData data;
  data.insert("tickets", tickets);
  data.insert("search", std::string(""));
  callback(drogon::HttpResponse::newHttpViewResponse("list", data));
}

void Requester::TicketDetails(const drogon::HttpRequestPtr &req,
                              Callback &&callback,
                              const std::string &ticket_id) {
  auto *logger = quill::Frontend::get_logger("root");
  const auto token = req->getCookie("token");

  sqlite3 *db = ConnectDB();
  if (!db)
    return BadRequest(callback, "Database unavailable",
                      drogon::k503ServiceUnavailable);

  const auto profile = FetchProfile(logger, db, token);
  if (!profile) {
    sqlite3_close(db);
    return BadRequest(callback, "Forbidden", drogon::k403Forbidden);
  }

  const auto ticket = FetchTicketDetails(logger, db, ticket_id, profile->id);
  if (!ticket) {
    sqlite3_close(db);
    return BadRequest(callback, "Ticket not found", drogon::k404NotFound);
  }

  auto activities = FetchActivities(logger, db, ticket_id);
  sqlite3_close(db);

  drogon::HttpViewData data;
  data.insert("ticket", *ticket);
  data.insert("activities", activities);
  callback(drogon::HttpResponse::newHttpViewResponse("details", data));
}

void Requester::TicketActivityList(const drogon::HttpRequestPtr &req,
                                   Callback &&callback,
                                   const std::string &ticket_id) {
  auto *logger = quill::Frontend::get_logger("root");
  const auto token = req->getCookie("token");

  sqlite3 *db = ConnectDB();
  if (!db)
    return BadRequest(callback, "Database unavailable",
                      drogon::k503ServiceUnavailable);

  const auto profile = FetchProfile(logger, db, token);
  if (!profile) {
    sqlite3_close(db);
    return BadRequest(callback, "Forbidden", drogon::k403Forbidden);
  }

  auto activities = FetchActivities(logger, db, ticket_id);
  sqlite3_close(db);

  drogon::HttpViewData data;
  data.insert("activities", activities);
  callback(drogon::HttpResponse::newHttpViewResponse("details", data));
}

void Requester::TicketActivityCreateMessage(const drogon::HttpRequestPtr &req,
                                            Callback &&callback,
                                            const std::string &ticket_id) {
  auto *logger = quill::Frontend::get_logger("root");
  const auto token = req->getCookie("token");

  const auto message = req->getParameter("message");
  if (message.empty())
    return BadRequest(callback, "Missing field: message");

  sqlite3 *db = ConnectDB();
  if (!db)
    return BadRequest(callback, "Database unavailable",
                      drogon::k503ServiceUnavailable);

  const auto profile = FetchProfile(logger, db, token);
  if (!profile) {
    sqlite3_close(db);
    return BadRequest(callback, "Invalid or expired token", drogon::k401Unauthorized);
  }

  sqlite3_stmt *stmt = nullptr;
  const char *sql = R"SQL(
INSERT INTO helpdesk_ticket_activity (ticket_id, profile_id, kind, body)
SELECT ?, p.id, 'message', ?
  FROM helpdesk_ticket t
  JOIN helpdesk_profile p ON p.id = ?
 WHERE t.id = ?
   AND t.requester_id = p.id
)SQL";

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    LOGJ_DEBUG(logger, "[requester] insert activity error", sqlite3_errmsg(db));
    sqlite3_close(db);
    return BadRequest(callback, "Failed to create activity");
  }
  sqlite3_bind_text(stmt, 1, ticket_id.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 2, message.c_str(),   -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 3, profile->id.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 4, ticket_id.c_str(), -1, SQLITE_STATIC);

  if (sqlite3_step(stmt) != SQLITE_DONE || sqlite3_changes(db) != 1) {
    LOGJ_DEBUG(logger, "[requester] insert activity step error", sqlite3_errmsg(db));
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return BadRequest(callback, "Failed to create activity");
  }
  sqlite3_finalize(stmt);

  const auto last_id = std::to_string(sqlite3_last_insert_rowid(db));
  sqlite3_close(db);

  Row activity{{"id", last_id}, {"body", message},
               {"created_at", "ahora"}, {"profile_name", profile->name}};

  drogon::HttpViewData data;
  data.insert("activity", activity);
  callback(drogon::HttpResponse::newHttpViewResponse("activity_message", data));
}

} // namespace ticketeer
