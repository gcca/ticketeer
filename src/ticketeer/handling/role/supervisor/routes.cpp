#include "routes.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <drogon/DrTemplateBase.h>
#include <drogon/MultiPart.h>
#include <json/json.h>
#include <quill/Frontend.h>
#include <quill/LogMacros.h>
#include <sqlite3.h>

#include "ticketeer/core/conf.hpp"

namespace {

using Callback = std::function<void(const drogon::HttpResponsePtr &)>;
using Row = std::map<std::string, std::string>;
using Rows = std::vector<Row>;

struct Profile {
  std::string id;
  std::string username;
  std::string name;
};

struct TicketCreateInput {
  std::string request_type_id;
  std::string department_id;
  std::string priority_id;
  std::string description;
  std::string due_date;
};

struct AttachmentFile {
  std::string file_path;
  std::string file_name;
  std::string mime_type;
};

inline constexpr std::size_t MaxActivityAttachmentSize = 25UL * 1024 * 1024;
inline constexpr std::size_t MaxTicketAttachmentSize = 75UL * 1024 * 1024;

inline void BadRequest(const Callback &callback, const char *msg,
                       drogon::HttpStatusCode code = drogon::k400BadRequest) {
  auto resp = drogon::HttpResponse::newHttpResponse();
  resp->setStatusCode(code);
  resp->setBody(msg);
  callback(resp);
}

[[nodiscard]] inline std::string ColumnText(sqlite3_stmt *stmt, int i) {
  const auto *v = sqlite3_column_text(stmt, i);
  return v ? reinterpret_cast<const char *>(v) : "";
}

[[nodiscard]] inline sqlite3 *ConnectDB() {
  std::string path = ticketeer::core::conf::settings.DB_URL;
  if (path.starts_with("sqlite:"))
    path = path.substr(7);
  sqlite3 *db = nullptr;
  if (sqlite3_open(path.c_str(), &db) != SQLITE_OK) {
    sqlite3_close(db);
    return nullptr;
  }
  return db;
}

[[nodiscard]] inline std::optional<Profile>
FetchProfile(quill::Logger *logger, sqlite3 *db, const std::string &token) {
  sqlite3_stmt *stmt = nullptr;
  const char *sql = R"SQL(
SELECT p.id, u.username, u.name
  FROM helpdesk_profile p
  JOIN auth_session s ON s.user_id = p.user_id
  JOIN auth_user u    ON u.id = p.user_id
 WHERE s.token = ?
   AND s.expires_at > datetime('now')
   AND p.role = 'supervisor'
 LIMIT 1
)SQL";

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    LOGJ_DEBUG(logger, "[supervisor] profile query error", sqlite3_errmsg(db));
    return std::nullopt;
  }
  sqlite3_bind_text(stmt, 1, token.c_str(), -1, SQLITE_STATIC);

  std::optional<Profile> profile;
  if (sqlite3_step(stmt) == SQLITE_ROW)
    profile =
        Profile{ColumnText(stmt, 0), ColumnText(stmt, 1), ColumnText(stmt, 2)};
  else
    LOGJ_DEBUG(logger, "[supervisor] profile not found");

  sqlite3_finalize(stmt);
  return profile;
}

[[nodiscard]] inline Rows FetchLookupRows(quill::Logger *logger, sqlite3 *db,
                                          const char *sql,
                                          const char *log_message) {
  sqlite3_stmt *stmt = nullptr;
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    LOGJ_DEBUG(logger, "[supervisor] lookup query error", log_message,
               sqlite3_errmsg(db));
    return {};
  }

  Rows rows;
  while (sqlite3_step(stmt) == SQLITE_ROW)
    rows.push_back(
        {{"id", ColumnText(stmt, 0)}, {"name", ColumnText(stmt, 1)}});

  sqlite3_finalize(stmt);
  return rows;
}

[[nodiscard]] inline Rows FetchRequestTypes(quill::Logger *logger,
                                            sqlite3 *db) {
  return FetchLookupRows(logger, db,
                         "SELECT id, name FROM helpdesk_request_type ORDER BY "
                         "name",
                         "[supervisor] request type list query error");
}

[[nodiscard]] inline Rows FetchRequestTypesDisplay(quill::Logger *logger,
                                                   sqlite3 *db) {
  return FetchLookupRows(logger, db,
                         "SELECT id, COALESCE(NULLIF(description,''), name) "
                         "FROM helpdesk_request_type ORDER BY name",
                         "[supervisor] request type display query error");
}

[[nodiscard]] inline Rows FetchDepartments(quill::Logger *logger, sqlite3 *db) {
  return FetchLookupRows(logger, db,
                         "SELECT id, name FROM helpdesk_department ORDER BY "
                         "name",
                         "[supervisor] department list query error");
}

[[nodiscard]] inline Rows FetchPriorities(quill::Logger *logger, sqlite3 *db) {
  return FetchLookupRows(logger, db,
                         "SELECT id, display_name FROM helpdesk_priority ORDER "
                         "BY id",
                         "[supervisor] priority list query error");
}

[[nodiscard]] inline Rows FetchStatuses(quill::Logger *logger, sqlite3 *db) {
  return FetchLookupRows(logger, db,
                         "SELECT id, display_name FROM "
                         "helpdesk_ticket_status ORDER BY id",
                         "[supervisor] status list query error");
}

[[nodiscard]] inline Rows FetchAssignees(quill::Logger *logger, sqlite3 *db) {
  return FetchLookupRows(logger, db,
                         "SELECT p.id, u.name || ' (' || u.username || ')' "
                         "FROM helpdesk_profile p "
                         "JOIN auth_user u ON u.id = p.user_id "
                         "WHERE p.role IN ('technician', 'supervisor') "
                         "ORDER BY u.name",
                         "[supervisor] assignee list query error");
}

[[nodiscard]] inline std::optional<std::string>
FetchDefaultStatusId(quill::Logger *logger, sqlite3 *db) {
  sqlite3_stmt *stmt = nullptr;
  if (sqlite3_prepare_v2(db,
                         "SELECT default_status_id FROM helpdesk_setting WHERE "
                         "name = 'default'",
                         -1, &stmt, nullptr) != SQLITE_OK) {
    LOGJ_DEBUG(logger, "[supervisor] default status query error",
               sqlite3_errmsg(db));
    return std::nullopt;
  }

  std::optional<std::string> id;
  if (sqlite3_step(stmt) == SQLITE_ROW)
    id = ColumnText(stmt, 0);

  sqlite3_finalize(stmt);
  return id;
}

[[nodiscard]] inline std::optional<std::string>
FetchDefaultAssignedToId(quill::Logger *logger, sqlite3 *db) {
  sqlite3_stmt *stmt = nullptr;
  if (sqlite3_prepare_v2(db,
                         "SELECT default_assigned_to_id FROM helpdesk_setting "
                         "WHERE name = 'default'",
                         -1, &stmt, nullptr) != SQLITE_OK) {
    LOGJ_DEBUG(logger, "[supervisor] default assigned_to query error",
               sqlite3_errmsg(db));
    return std::nullopt;
  }

  std::optional<std::string> id;
  if (sqlite3_step(stmt) == SQLITE_ROW)
    id = ColumnText(stmt, 0);

  sqlite3_finalize(stmt);
  return id;
}

[[nodiscard]] inline Rows FetchTickets(quill::Logger *logger, sqlite3 *db,
                                       const std::string &search,
                                       const std::string &department_id) {
  sqlite3_stmt *stmt = nullptr;
  std::string sql = R"SQL(
SELECT t.id, t.description, ts.display_name, p.display_name, rt.name, d.name,
       ru.name, COALESCE(au.name, ''), t.created_at, COALESCE(t.due_date, '')
  FROM helpdesk_ticket t
  JOIN helpdesk_ticket_status ts ON ts.id = t.status_id
  JOIN helpdesk_priority p       ON p.id  = t.priority_id
  JOIN helpdesk_request_type rt  ON rt.id = t.request_type_id
  JOIN helpdesk_department d     ON d.id  = t.department_id
  JOIN helpdesk_profile rp       ON rp.id = t.requester_id
  JOIN auth_user ru              ON ru.id = rp.user_id
  LEFT JOIN helpdesk_profile ap  ON ap.id = t.assigned_to_id
  LEFT JOIN auth_user au         ON au.id = ap.user_id
)SQL";

  if (!search.empty()) {
    sql += R"SQL(
 WHERE (t.description LIKE ?
    OR ts.display_name LIKE ?
    OR p.display_name LIKE ?
    OR rt.name LIKE ?
    OR d.name LIKE ?
    OR ru.name LIKE ?
    OR au.name LIKE ?)
)SQL";
  }
  if (!department_id.empty())
    sql += search.empty() ? " WHERE d.id = ?" : " AND d.id = ?";

  sql += " ORDER BY t.created_at DESC LIMIT 100";

  if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
    LOGJ_DEBUG(logger, "[supervisor] ticket list query error",
               sqlite3_errmsg(db));
    return {};
  }

  int bind_idx = 1;
  std::string like_search;
  if (!search.empty()) {
    like_search = "%" + search + "%";
    for (int i = 0; i < 7; ++i)
      sqlite3_bind_text(stmt, bind_idx++, like_search.c_str(), -1,
                        SQLITE_STATIC);
  }
  if (!department_id.empty())
    sqlite3_bind_text(stmt, bind_idx, department_id.c_str(), -1, SQLITE_STATIC);

  Rows tickets;
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    tickets.push_back({{"id", ColumnText(stmt, 0)},
                       {"description", ColumnText(stmt, 1)},
                       {"status_name", ColumnText(stmt, 2)},
                       {"priority_name", ColumnText(stmt, 3)},
                       {"request_type_name", ColumnText(stmt, 4)},
                       {"department_name", ColumnText(stmt, 5)},
                       {"requester_name", ColumnText(stmt, 6)},
                       {"assigned_to_name", ColumnText(stmt, 7)},
                       {"created_at", ColumnText(stmt, 8)},
                       {"due_date", ColumnText(stmt, 9)}});
  }

  sqlite3_finalize(stmt);
  return tickets;
}

[[nodiscard]] inline std::optional<Row>
FetchTicketDetails(quill::Logger *logger, sqlite3 *db,
                   const std::string &ticket_id) {
  sqlite3_stmt *stmt = nullptr;
  const char *sql = R"SQL(
SELECT t.id, t.description, t.status_id, ts.display_name, p.display_name,
       rt.name, d.name, t.created_at, COALESCE(t.due_date, ''),
       COALESCE(au.name, '') AS assigned_to_name,
       ru.name AS requester_name,
       ru.username AS requester_username,
       COALESCE(au.username, '') AS assigned_to_username,
       t.department_id,
       COALESCE(t.assigned_to_id, '') AS assigned_to_id,
       t.request_type_id,
       COALESCE(NULLIF(rt.description, ''), rt.name) AS request_type_display,
       t.priority_id
  FROM helpdesk_ticket t
  JOIN helpdesk_ticket_status ts ON ts.id = t.status_id
  JOIN helpdesk_priority p       ON p.id  = t.priority_id
  JOIN helpdesk_request_type rt  ON rt.id = t.request_type_id
  JOIN helpdesk_department d     ON d.id  = t.department_id
  JOIN helpdesk_profile rp       ON rp.id = t.requester_id
  JOIN auth_user ru              ON ru.id = rp.user_id
  LEFT JOIN helpdesk_profile ap  ON ap.id = t.assigned_to_id
  LEFT JOIN auth_user au         ON au.id = ap.user_id
 WHERE t.id = ?
 LIMIT 1
)SQL";

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    LOGJ_DEBUG(logger, "[supervisor] ticket details query error",
               sqlite3_errmsg(db));
    return std::nullopt;
  }
  sqlite3_bind_text(stmt, 1, ticket_id.c_str(), -1, SQLITE_STATIC);

  std::optional<Row> ticket;
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    ticket = Row{{"id", ColumnText(stmt, 0)},
                 {"description", ColumnText(stmt, 1)},
                 {"status_id", ColumnText(stmt, 2)},
                 {"status_name", ColumnText(stmt, 3)},
                 {"priority_name", ColumnText(stmt, 4)},
                 {"request_type_name", ColumnText(stmt, 5)},
                 {"department_name", ColumnText(stmt, 6)},
                 {"created_at", ColumnText(stmt, 7)},
                 {"due_date", ColumnText(stmt, 8)},
                 {"assigned_to_name", ColumnText(stmt, 9)},
                 {"requester_name", ColumnText(stmt, 10)},
                 {"requester_username", ColumnText(stmt, 11)},
                 {"assigned_to_username", ColumnText(stmt, 12)},
                 {"department_id", ColumnText(stmt, 13)},
                 {"assigned_to_id", ColumnText(stmt, 14)},
                 {"request_type_id", ColumnText(stmt, 15)},
                 {"request_type_display", ColumnText(stmt, 16)},
                 {"priority_id", ColumnText(stmt, 17)}};
  }

  sqlite3_finalize(stmt);
  return ticket;
}

[[nodiscard]] inline std::optional<std::string>
FetchStatusName(quill::Logger *logger, sqlite3 *db, const std::string &id) {
  sqlite3_stmt *stmt = nullptr;
  if (sqlite3_prepare_v2(db,
                         "SELECT display_name FROM helpdesk_ticket_status "
                         "WHERE id = ?",
                         -1, &stmt, nullptr) != SQLITE_OK) {
    LOGJ_DEBUG(logger, "[supervisor] status name query error",
               sqlite3_errmsg(db));
    return std::nullopt;
  }
  sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_STATIC);

  std::optional<std::string> name;
  if (sqlite3_step(stmt) == SQLITE_ROW)
    name = ColumnText(stmt, 0);

  sqlite3_finalize(stmt);
  return name;
}

[[nodiscard]] inline bool UpdateTicketStatus(quill::Logger *logger, sqlite3 *db,
                                             const std::string &ticket_id,
                                             const std::string &new_status) {
  sqlite3_stmt *stmt = nullptr;
  const char *sql = R"SQL(
UPDATE helpdesk_ticket
   SET status_id = ?
 WHERE id = ?
   AND EXISTS (
       SELECT 1
         FROM helpdesk_ticket_status
        WHERE id = ?
   )
)SQL";

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    LOGJ_DEBUG(logger, "[supervisor] update status query error",
               sqlite3_errmsg(db));
    return false;
  }

  sqlite3_bind_text(stmt, 1, new_status.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 2, ticket_id.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 3, new_status.c_str(), -1, SQLITE_STATIC);
  const bool ok = sqlite3_step(stmt) == SQLITE_DONE && sqlite3_changes(db) == 1;
  sqlite3_finalize(stmt);
  return ok;
}

[[nodiscard]] inline bool
UpdateTicketDepartment(quill::Logger *logger, sqlite3 *db,
                       const std::string &ticket_id,
                       const std::string &new_department) {
  sqlite3_stmt *stmt = nullptr;
  const char *sql = R"SQL(
UPDATE helpdesk_ticket
   SET department_id = ?
 WHERE id = ?
   AND EXISTS (
       SELECT 1
         FROM helpdesk_department
        WHERE id = ?
   )
)SQL";

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    LOGJ_DEBUG(logger, "[supervisor] update department query error",
               sqlite3_errmsg(db));
    return false;
  }

  sqlite3_bind_text(stmt, 1, new_department.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 2, ticket_id.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 3, new_department.c_str(), -1, SQLITE_STATIC);
  const bool ok = sqlite3_step(stmt) == SQLITE_DONE && sqlite3_changes(db) == 1;
  sqlite3_finalize(stmt);
  return ok;
}

[[nodiscard]] inline bool
UpdateTicketRequestType(quill::Logger *logger, sqlite3 *db,
                        const std::string &ticket_id,
                        const std::string &new_request_type) {
  sqlite3_stmt *stmt = nullptr;
  const char *sql = R"SQL(
UPDATE helpdesk_ticket
   SET request_type_id = ?
 WHERE id = ?
   AND EXISTS (
       SELECT 1
         FROM helpdesk_request_type
        WHERE id = ?
   )
)SQL";

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    LOGJ_DEBUG(logger, "[supervisor] update request type query error",
               sqlite3_errmsg(db));
    return false;
  }

  sqlite3_bind_text(stmt, 1, new_request_type.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 2, ticket_id.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 3, new_request_type.c_str(), -1, SQLITE_STATIC);
  const bool ok = sqlite3_step(stmt) == SQLITE_DONE && sqlite3_changes(db) == 1;
  sqlite3_finalize(stmt);
  return ok;
}

[[nodiscard]] inline std::optional<std::string>
FetchPriorityName(quill::Logger *logger, sqlite3 *db, const std::string &id) {
  sqlite3_stmt *stmt = nullptr;
  if (sqlite3_prepare_v2(
          db, "SELECT display_name FROM helpdesk_priority WHERE id = ?", -1,
          &stmt, nullptr) != SQLITE_OK) {
    LOGJ_DEBUG(logger, "[supervisor] priority name query error",
               sqlite3_errmsg(db));
    return std::nullopt;
  }
  sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_STATIC);

  std::optional<std::string> name;
  if (sqlite3_step(stmt) == SQLITE_ROW)
    name = ColumnText(stmt, 0);

  sqlite3_finalize(stmt);
  return name;
}

inline void InsertTicketActivity(quill::Logger *logger, sqlite3 *db,
                                 const std::string &ticket_id,
                                 const std::string &profile_id,
                                 const std::string &kind,
                                 const std::string &body) {
  sqlite3_stmt *stmt = nullptr;
  const char *sql = R"SQL(
INSERT INTO helpdesk_ticket_activity (ticket_id, profile_id, kind, body)
VALUES (?, ?, ?, ?)
)SQL";
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    LOGJ_DEBUG(logger, "[supervisor] activity insert error",
               sqlite3_errmsg(db));
    return;
  }
  static const std::unordered_map<std::string, std::string> kind_es{
      {"status_changed", "Cambio de estado"},
      {"priority_changed", "Cambio de prioridad"},
      {"department_changed", "Cambio de área"},
      {"request_type_changed", "Cambio de tipo"},
      {"assigned_changed", "Cambio de asignado"},
      {"due_date_changed", "Cambio de fecha límite"},
      {"message", "Mensaje"},
  };
  const auto it = kind_es.find(kind);
  const std::string kind_display = it != kind_es.end() ? it->second : kind;
  const std::string full_body = kind_display + ": " + body;
  sqlite3_bind_text(stmt, 1, ticket_id.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 2, profile_id.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 3, kind.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 4, full_body.c_str(), -1, SQLITE_STATIC);
  sqlite3_step(stmt);
  sqlite3_finalize(stmt);
}

[[nodiscard]] inline bool
UpdateTicketPriority(quill::Logger *logger, sqlite3 *db,
                     const std::string &ticket_id,
                     const std::string &new_priority) {
  sqlite3_stmt *stmt = nullptr;
  const char *sql = R"SQL(
UPDATE helpdesk_ticket
   SET priority_id = ?
 WHERE id = ?
   AND EXISTS (
       SELECT 1
         FROM helpdesk_priority
        WHERE id = ?
   )
)SQL";

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    LOGJ_DEBUG(logger, "[supervisor] update priority query error",
               sqlite3_errmsg(db));
    return false;
  }

  sqlite3_bind_text(stmt, 1, new_priority.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 2, ticket_id.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 3, new_priority.c_str(), -1, SQLITE_STATIC);
  const bool ok = sqlite3_step(stmt) == SQLITE_DONE && sqlite3_changes(db) == 1;
  sqlite3_finalize(stmt);
  return ok;
}

[[nodiscard]] inline bool
UpdateTicketAssignedTo(quill::Logger *logger, sqlite3 *db,
                       const std::string &ticket_id,
                       const std::string &new_assigned_to) {
  sqlite3_stmt *stmt = nullptr;
  const char *sql = R"SQL(
UPDATE helpdesk_ticket
   SET assigned_to_id = NULLIF(?, '')
 WHERE id = ?
   AND (? = '' OR EXISTS (
       SELECT 1 FROM helpdesk_profile WHERE id = ? AND role != 'requester'
   ))
)SQL";

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    LOGJ_DEBUG(logger, "[supervisor] update assigned_to query error",
               sqlite3_errmsg(db));
    return false;
  }

  sqlite3_bind_text(stmt, 1, new_assigned_to.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 2, ticket_id.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 3, new_assigned_to.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 4, new_assigned_to.c_str(), -1, SQLITE_STATIC);
  const bool ok = sqlite3_step(stmt) == SQLITE_DONE && sqlite3_changes(db) == 1;
  sqlite3_finalize(stmt);
  return ok;
}

[[nodiscard]] inline Rows FetchActivities(quill::Logger *logger, sqlite3 *db,
                                          const std::string &ticket_id) {
  sqlite3_stmt *stmt = nullptr;
  const char *sql = R"SQL(
SELECT ta.id, ta.kind, ta.body, ta.created_at, u.name
  FROM helpdesk_ticket_activity ta
  JOIN helpdesk_profile p ON p.id = ta.profile_id
  JOIN auth_user u        ON u.id = p.user_id
 WHERE ta.ticket_id = ?
 ORDER BY ta.created_at ASC, ta.id ASC
)SQL";

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    LOGJ_DEBUG(logger, "[supervisor] activities query error",
               sqlite3_errmsg(db));
    return {};
  }
  sqlite3_bind_text(stmt, 1, ticket_id.c_str(), -1, SQLITE_STATIC);

  Rows activities;
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    activities.push_back({{"id", ColumnText(stmt, 0)},
                          {"kind", ColumnText(stmt, 1)},
                          {"body", ColumnText(stmt, 2)},
                          {"created_at", ColumnText(stmt, 3)},
                          {"profile_name", ColumnText(stmt, 4)}});
  }

  sqlite3_finalize(stmt);
  return activities;
}

[[nodiscard]] inline Rows FetchAttachments(quill::Logger *logger, sqlite3 *db,
                                           const std::string &ticket_id) {
  sqlite3_stmt *stmt = nullptr;
  const char *sql = R"SQL(
SELECT taa.id, taa.ticket_activity_id, taa.file_name,
       taa.file_size, taa.mime_type
  FROM helpdesk_ticket_activity_attachment taa
  JOIN helpdesk_ticket_activity ta ON ta.id = taa.ticket_activity_id
 WHERE ta.ticket_id = ?
 ORDER BY taa.created_at ASC, taa.id ASC
)SQL";

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    LOGJ_DEBUG(logger, "[supervisor] attachments query error",
               sqlite3_errmsg(db));
    return {};
  }
  sqlite3_bind_text(stmt, 1, ticket_id.c_str(), -1, SQLITE_STATIC);

  Rows attachments;
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    attachments.push_back({{"id", ColumnText(stmt, 0)},
                           {"activity_id", ColumnText(stmt, 1)},
                           {"file_name", ColumnText(stmt, 2)},
                           {"file_size", ColumnText(stmt, 3)},
                           {"mime_type", ColumnText(stmt, 4)}});
  }

  sqlite3_finalize(stmt);
  return attachments;
}

[[nodiscard]] inline bool TicketExists(quill::Logger *logger, sqlite3 *db,
                                       const std::string &ticket_id) {
  sqlite3_stmt *stmt = nullptr;
  if (sqlite3_prepare_v2(db, "SELECT id FROM helpdesk_ticket WHERE id = ?", -1,
                         &stmt, nullptr) != SQLITE_OK) {
    LOGJ_DEBUG(logger, "[supervisor] ticket query error", sqlite3_errmsg(db));
    return false;
  }

  sqlite3_bind_text(stmt, 1, ticket_id.c_str(), -1, SQLITE_STATIC);
  const bool exists = sqlite3_step(stmt) == SQLITE_ROW;
  sqlite3_finalize(stmt);
  return exists;
}

[[nodiscard]] inline std::size_t
FetchActivityAttachmentTotal(sqlite3 *db, const std::string &activity_id) {
  sqlite3_stmt *stmt = nullptr;
  if (sqlite3_prepare_v2(
          db,
          "SELECT COALESCE(SUM(file_size),0) FROM "
          "helpdesk_ticket_activity_attachment WHERE ticket_activity_id = ?",
          -1, &stmt, nullptr) != SQLITE_OK)
    return 0;
  sqlite3_bind_text(stmt, 1, activity_id.c_str(), -1, SQLITE_STATIC);
  std::size_t total = 0;
  if (sqlite3_step(stmt) == SQLITE_ROW)
    total = static_cast<std::size_t>(sqlite3_column_int64(stmt, 0));
  sqlite3_finalize(stmt);
  return total;
}

[[nodiscard]] inline std::size_t
FetchTicketAttachmentTotal(sqlite3 *db, const std::string &ticket_id) {
  sqlite3_stmt *stmt = nullptr;
  const char *sql = R"SQL(
SELECT COALESCE(SUM(taa.file_size), 0)
  FROM helpdesk_ticket_activity_attachment taa
  JOIN helpdesk_ticket_activity ta ON ta.id = taa.ticket_activity_id
 WHERE ta.ticket_id = ?
)SQL";
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK)
    return 0;
  sqlite3_bind_text(stmt, 1, ticket_id.c_str(), -1, SQLITE_STATIC);
  std::size_t total = 0;
  if (sqlite3_step(stmt) == SQLITE_ROW)
    total = static_cast<std::size_t>(sqlite3_column_int64(stmt, 0));
  sqlite3_finalize(stmt);
  return total;
}

[[nodiscard]] inline bool TicketActivityExists(quill::Logger *logger,
                                               sqlite3 *db,
                                               const std::string &ticket_id,
                                               const std::string &activity_id) {
  sqlite3_stmt *stmt = nullptr;
  const char *sql = R"SQL(
SELECT id
  FROM helpdesk_ticket_activity
 WHERE ticket_id = ?
   AND id = ?
)SQL";

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    LOGJ_DEBUG(logger, "[supervisor] ticket activity query error",
               sqlite3_errmsg(db));
    return false;
  }

  sqlite3_bind_text(stmt, 1, ticket_id.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 2, activity_id.c_str(), -1, SQLITE_STATIC);
  const bool exists = sqlite3_step(stmt) == SQLITE_ROW;
  sqlite3_finalize(stmt);
  return exists;
}

[[nodiscard]] inline std::optional<TicketCreateInput>
ParseTicketCreateInput(const drogon::HttpRequestPtr &req) {
  if (const auto body = req->getJsonObject()) {
    const auto &request_type_id = (*body)["request_type_id"];
    const auto &department_id = (*body)["department_id"];
    const auto &priority_id = (*body)["priority_id"];
    const auto &description = (*body)["description"];
    const auto &due_date = (*body)["due_date"];

    if (!request_type_id.isString() || !department_id.isString() ||
        !priority_id.isString() || !description.isString())
      return std::nullopt;

    TicketCreateInput input{request_type_id.asString(),
                            department_id.asString(), priority_id.asString(),
                            description.asString(),
                            due_date.isString() ? due_date.asString() : ""};
    if (input.request_type_id.empty() || input.department_id.empty() ||
        input.priority_id.empty() || input.description.empty())
      return std::nullopt;
    return input;
  }

  TicketCreateInput input{
      req->getParameter("request_type_id"), req->getParameter("department_id"),
      req->getParameter("priority_id"), req->getParameter("description"),
      req->getParameter("due_date")};
  if (input.request_type_id.empty() || input.department_id.empty() ||
      input.priority_id.empty() || input.description.empty())
    return std::nullopt;
  return input;
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
  if (ec)
    return false;
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  if (!out)
    return false;
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

[[nodiscard]] inline std::optional<AttachmentFile>
FetchTicketActivityAttachmentFile(quill::Logger *logger, sqlite3 *db,
                                  const std::string &ticket_id,
                                  const std::string &activity_id,
                                  const std::string &attachment_id) {
  sqlite3_stmt *stmt = nullptr;
  const char *sql = R"SQL(
SELECT taa.file_path, taa.file_name, taa.mime_type
  FROM helpdesk_ticket_activity_attachment taa
  JOIN helpdesk_ticket_activity ta ON ta.id = taa.ticket_activity_id
 WHERE ta.ticket_id = ?
   AND ta.id = ?
   AND taa.id = ?
)SQL";

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    LOGJ_DEBUG(logger, "[supervisor] attachment download query error",
               sqlite3_errmsg(db));
    return std::nullopt;
  }

  sqlite3_bind_text(stmt, 1, ticket_id.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 2, activity_id.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 3, attachment_id.c_str(), -1, SQLITE_STATIC);

  std::optional<AttachmentFile> file;
  if (sqlite3_step(stmt) == SQLITE_ROW)
    file = AttachmentFile{ColumnText(stmt, 0), ColumnText(stmt, 1),
                          ColumnText(stmt, 2)};

  sqlite3_finalize(stmt);
  return file;
}

inline bool Exec(sqlite3 *db, const char *sql) {
  return sqlite3_exec(db, sql, nullptr, nullptr, nullptr) == SQLITE_OK;
}

} // namespace

namespace ticketeer {

void Supervisor::HomeGet(const drogon::HttpRequestPtr &req,
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
    return callback(
        drogon::HttpResponse::newRedirectionResponse("/ticketeer/auth/signin"));

  drogon::HttpViewData data;
  data.insert("username", profile->username);
  data.insert("name", profile->name);
  callback(drogon::HttpResponse::newHttpViewResponse("supervisor_home", data));
}

void Supervisor::TicketListGet(const drogon::HttpRequestPtr &req,
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
    return callback(
        drogon::HttpResponse::newRedirectionResponse("/ticketeer/auth/signin"));
  }

  const auto search = req->getParameter("s");
  const auto department_id = req->getParameter("d");
  auto tickets = FetchTickets(logger, db, search, department_id);
  auto departments = FetchDepartments(logger, db);
  sqlite3_close(db);

  drogon::HttpViewData data;
  data.insert("tickets", tickets);
  data.insert("search", search);
  data.insert("departments", departments);
  data.insert("department_id", department_id);
  callback(drogon::HttpResponse::newHttpViewResponse("supervisor_ticket_list",
                                                     data));
}

void Supervisor::TicketCreateGet(const drogon::HttpRequestPtr &req,
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
    return callback(
        drogon::HttpResponse::newRedirectionResponse("/ticketeer/auth/signin"));
  }

  auto request_types = FetchRequestTypes(logger, db);
  auto departments = FetchDepartments(logger, db);
  auto priorities = FetchPriorities(logger, db);
  sqlite3_close(db);

  drogon::HttpViewData data;
  data.insert("request_types", request_types);
  data.insert("departments", departments);
  data.insert("priorities", priorities);
  callback(drogon::HttpResponse::newHttpViewResponse("supervisor_ticket_create",
                                                     data));
}

void Supervisor::TicketCreatePost(const drogon::HttpRequestPtr &req,
                                  Callback &&callback) {
  auto *logger = quill::Frontend::get_logger("root");
  const auto token = req->getCookie("token");

  const auto input = ParseTicketCreateInput(req);
  if (!input)
    return BadRequest(callback, "Missing or invalid required fields: "
                                "request_type_id, department_id, priority_id, "
                                "description");

  sqlite3 *db = ConnectDB();
  if (!db)
    return BadRequest(callback, "Database unavailable",
                      drogon::k503ServiceUnavailable);

  const auto profile = FetchProfile(logger, db, token);
  if (!profile) {
    sqlite3_close(db);
    return BadRequest(callback, "Invalid or expired token",
                      drogon::k401Unauthorized);
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
  const std::string &due = input->due_date;
  if (!due.empty())
    sql += ", due_date";
  sql += ") VALUES (?, ?, ?, ?, ?, ?, ?";
  if (!due.empty())
    sql += ", ?";
  sql += ")";

  if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
    LOGJ_DEBUG(logger, "[supervisor] insert ticket error", sqlite3_errmsg(db));
    sqlite3_close(db);
    return BadRequest(callback, "Failed to create ticket");
  }

  sqlite3_bind_text(stmt, 1, input->request_type_id.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 2, profile->id.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 3, input->department_id.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 4, input->priority_id.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 5, default_status_id->c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 6, default_assigned_to_id->c_str(), -1,
                    SQLITE_STATIC);
  sqlite3_bind_text(stmt, 7, input->description.c_str(), -1, SQLITE_STATIC);
  if (!due.empty())
    sqlite3_bind_text(stmt, 8, input->due_date.c_str(), -1, SQLITE_STATIC);

  if (sqlite3_step(stmt) != SQLITE_DONE) {
    LOGJ_DEBUG(logger, "[supervisor] insert ticket step error",
               sqlite3_errmsg(db));
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return BadRequest(callback, "Failed to create ticket");
  }
  sqlite3_finalize(stmt);

  auto tickets = FetchTickets(logger, db, "", "");
  auto departments = FetchDepartments(logger, db);
  sqlite3_close(db);

  drogon::HttpViewData data;
  data.insert("tickets", tickets);
  data.insert("search", std::string(""));
  data.insert("departments", departments);
  data.insert("department_id", std::string(""));
  callback(drogon::HttpResponse::newHttpViewResponse("supervisor_ticket_list",
                                                     data));
}

void Supervisor::TicketDetailsGet(const drogon::HttpRequestPtr &req,
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

  const auto ticket = FetchTicketDetails(logger, db, ticket_id);
  if (!ticket) {
    sqlite3_close(db);
    return BadRequest(callback, "Ticket not found", drogon::k404NotFound);
  }

  auto activities = FetchActivities(logger, db, ticket_id);
  auto attachments = FetchAttachments(logger, db, ticket_id);
  auto statuses = FetchStatuses(logger, db);
  auto departments = FetchDepartments(logger, db);
  auto assignees = FetchAssignees(logger, db);
  auto request_types = FetchRequestTypesDisplay(logger, db);
  auto priorities = FetchPriorities(logger, db);
  sqlite3_close(db);

  drogon::HttpViewData data;
  data.insert("ticket", *ticket);
  data.insert("activities", activities);
  data.insert("attachments", attachments);
  data.insert("statuses", statuses);
  data.insert("departments", departments);
  data.insert("assignees", assignees);
  data.insert("request_types", request_types);
  data.insert("priorities", priorities);
  callback(drogon::HttpResponse::newHttpViewResponse(
      "supervisor_ticket_details", data));
}

void Supervisor::TicketStatusPost(const drogon::HttpRequestPtr &req,
                                  Callback &&callback,
                                  const std::string &ticket_id) {
  auto *logger = quill::Frontend::get_logger("root");
  const auto token = req->getCookie("token");
  const auto new_status = req->getParameter("new_status");
  if (new_status.empty())
    return BadRequest(callback, "Missing field: new_status");

  sqlite3 *db = ConnectDB();
  if (!db)
    return BadRequest(callback, "Database unavailable",
                      drogon::k503ServiceUnavailable);

  const auto profile = FetchProfile(logger, db, token);
  if (!profile) {
    sqlite3_close(db);
    return BadRequest(callback, "Forbidden", drogon::k403Forbidden);
  }

  if (!UpdateTicketStatus(logger, db, ticket_id, new_status)) {
    sqlite3_close(db);
    return BadRequest(callback, "Failed to update status",
                      drogon::k400BadRequest);
  }

  const auto status_name = FetchStatusName(logger, db, new_status);
  if (!status_name) {
    sqlite3_close(db);
    return BadRequest(callback, "Status not found", drogon::k404NotFound);
  }
  InsertTicketActivity(logger, db, ticket_id, profile->id, "status_changed",
                       *status_name);
  sqlite3_close(db);

  drogon::HttpViewData data;
  data.insert("status_name", *status_name);
  callback(drogon::HttpResponse::newHttpViewResponse(
      "supervisor_ticket_status_badge", data));
}

void Supervisor::TicketDepartmentPost(const drogon::HttpRequestPtr &req,
                                      Callback &&callback,
                                      const std::string &ticket_id) {
  auto *logger = quill::Frontend::get_logger("root");
  const auto token = req->getCookie("token");
  const auto new_department = req->getParameter("new_department");
  if (new_department.empty())
    return BadRequest(callback, "Missing field: new_department");

  sqlite3 *db = ConnectDB();
  if (!db)
    return BadRequest(callback, "Database unavailable",
                      drogon::k503ServiceUnavailable);

  const auto profile = FetchProfile(logger, db, token);
  if (!profile) {
    sqlite3_close(db);
    return BadRequest(callback, "Forbidden", drogon::k403Forbidden);
  }

  if (!UpdateTicketDepartment(logger, db, ticket_id, new_department)) {
    sqlite3_close(db);
    return BadRequest(callback, "Failed to update department",
                      drogon::k400BadRequest);
  }

  auto departments = FetchDepartments(logger, db);
  std::string dept_name;
  for (const auto &d : departments)
    if (d.at("id") == new_department) {
      dept_name = d.at("name");
      break;
    }
  InsertTicketActivity(logger, db, ticket_id, profile->id, "department_changed",
                       dept_name);
  sqlite3_close(db);

  drogon::HttpViewData data;
  data.insert("ticket_id", ticket_id);
  data.insert("department_id", new_department);
  data.insert("departments", departments);
  callback(drogon::HttpResponse::newHttpViewResponse(
      "supervisor_ticket_department_select", data));
}

void Supervisor::TicketAssignedToPost(const drogon::HttpRequestPtr &req,
                                      Callback &&callback,
                                      const std::string &ticket_id) {
  auto *logger = quill::Frontend::get_logger("root");
  const auto token = req->getCookie("token");
  const auto new_assigned_to = req->getParameter("new_assigned_to");

  sqlite3 *db = ConnectDB();
  if (!db)
    return BadRequest(callback, "Database unavailable",
                      drogon::k503ServiceUnavailable);

  const auto profile = FetchProfile(logger, db, token);
  if (!profile) {
    sqlite3_close(db);
    return BadRequest(callback, "Forbidden", drogon::k403Forbidden);
  }

  if (!UpdateTicketAssignedTo(logger, db, ticket_id, new_assigned_to)) {
    sqlite3_close(db);
    return BadRequest(callback, "Failed to update assigned to",
                      drogon::k400BadRequest);
  }

  auto assignees = FetchAssignees(logger, db);
  std::string assignee_name = "Sin asignar";
  for (const auto &a : assignees)
    if (a.at("id") == new_assigned_to) {
      assignee_name = a.at("name");
      break;
    }
  InsertTicketActivity(logger, db, ticket_id, profile->id, "assigned_changed",
                       assignee_name);
  sqlite3_close(db);

  drogon::HttpViewData data;
  data.insert("ticket_id", ticket_id);
  data.insert("assigned_to_id", new_assigned_to);
  data.insert("assignees", assignees);
  callback(drogon::HttpResponse::newHttpViewResponse(
      "supervisor_ticket_assigned_to_select", data));
}

void Supervisor::TicketRequestTypePost(const drogon::HttpRequestPtr &req,
                                       Callback &&callback,
                                       const std::string &ticket_id) {
  auto *logger = quill::Frontend::get_logger("root");
  const auto token = req->getCookie("token");
  const auto new_request_type = req->getParameter("new_request_type");
  if (new_request_type.empty())
    return BadRequest(callback, "Missing field: new_request_type");

  sqlite3 *db = ConnectDB();
  if (!db)
    return BadRequest(callback, "Database unavailable",
                      drogon::k503ServiceUnavailable);

  const auto profile = FetchProfile(logger, db, token);
  if (!profile) {
    sqlite3_close(db);
    return BadRequest(callback, "Forbidden", drogon::k403Forbidden);
  }

  if (!UpdateTicketRequestType(logger, db, ticket_id, new_request_type)) {
    sqlite3_close(db);
    return BadRequest(callback, "Failed to update request type",
                      drogon::k400BadRequest);
  }

  auto request_types = FetchRequestTypesDisplay(logger, db);
  std::string rt_name;
  for (const auto &rt : request_types)
    if (rt.at("id") == new_request_type) {
      rt_name = rt.at("name");
      break;
    }
  InsertTicketActivity(logger, db, ticket_id, profile->id,
                       "request_type_changed", rt_name);
  sqlite3_close(db);

  drogon::HttpViewData data;
  data.insert("ticket_id", ticket_id);
  data.insert("request_type_id", new_request_type);
  data.insert("request_types", request_types);
  callback(drogon::HttpResponse::newHttpViewResponse(
      "supervisor_ticket_request_type_select", data));
}

void Supervisor::TicketPriorityPost(const drogon::HttpRequestPtr &req,
                                    Callback &&callback,
                                    const std::string &ticket_id) {
  auto *logger = quill::Frontend::get_logger("root");
  const auto token = req->getCookie("token");
  const auto new_priority = req->getParameter("new_priority");
  if (new_priority.empty())
    return BadRequest(callback, "Missing field: new_priority");

  sqlite3 *db = ConnectDB();
  if (!db)
    return BadRequest(callback, "Database unavailable",
                      drogon::k503ServiceUnavailable);

  const auto profile = FetchProfile(logger, db, token);
  if (!profile) {
    sqlite3_close(db);
    return BadRequest(callback, "Forbidden", drogon::k403Forbidden);
  }

  if (!UpdateTicketPriority(logger, db, ticket_id, new_priority)) {
    sqlite3_close(db);
    return BadRequest(callback, "Failed to update priority",
                      drogon::k400BadRequest);
  }

  const auto priority_name = FetchPriorityName(logger, db, new_priority);
  if (!priority_name) {
    sqlite3_close(db);
    return BadRequest(callback, "Priority not found", drogon::k404NotFound);
  }
  InsertTicketActivity(logger, db, ticket_id, profile->id, "priority_changed",
                       *priority_name);
  sqlite3_close(db);

  drogon::HttpViewData data;
  data.insert("priority_name", *priority_name);
  callback(drogon::HttpResponse::newHttpViewResponse(
      "supervisor_ticket_priority_badge", data));
}

void Supervisor::TicketActivityListGet(const drogon::HttpRequestPtr &req,
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

  if (!TicketExists(logger, db, ticket_id)) {
    sqlite3_close(db);
    return BadRequest(callback, "Ticket not found", drogon::k404NotFound);
  }

  auto activities = FetchActivities(logger, db, ticket_id);
  auto attachments = FetchAttachments(logger, db, ticket_id);
  sqlite3_close(db);

  drogon::HttpViewData data;
  data.insert("ticket_id", ticket_id);
  data.insert("activities", activities);
  data.insert("attachments", attachments);
  callback(drogon::HttpResponse::newHttpViewResponse(
      "supervisor_ticket_activity_list", data));
}

void Supervisor::TicketActivityCreateMessagePost(
    const drogon::HttpRequestPtr &req, Callback &&callback,
    const std::string &ticket_id) {
  auto *logger = quill::Frontend::get_logger("root");
  const auto token = req->getCookie("token");

  std::string message = req->getParameter("message");
  if (message.empty()) {
    if (const auto body = req->getJsonObject()) {
      const auto &json_message = (*body)["message"];
      if (json_message.isString())
        message = json_message.asString();
    }
  }
  if (message.empty())
    return BadRequest(callback, "Missing field: message");

  sqlite3 *db = ConnectDB();
  if (!db)
    return BadRequest(callback, "Database unavailable",
                      drogon::k503ServiceUnavailable);

  const auto profile = FetchProfile(logger, db, token);
  if (!profile) {
    sqlite3_close(db);
    return BadRequest(callback, "Invalid or expired token",
                      drogon::k401Unauthorized);
  }

  if (!TicketExists(logger, db, ticket_id)) {
    sqlite3_close(db);
    return BadRequest(callback, "Ticket not found", drogon::k404NotFound);
  }

  sqlite3_stmt *stmt = nullptr;
  const char *sql = R"SQL(
INSERT INTO helpdesk_ticket_activity (ticket_id, profile_id, kind, body)
VALUES (?, ?, 'message', ?)
)SQL";

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    LOGJ_DEBUG(logger, "[supervisor] insert activity error",
               sqlite3_errmsg(db));
    sqlite3_close(db);
    return BadRequest(callback, "Failed to create activity");
  }
  sqlite3_bind_text(stmt, 1, ticket_id.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 2, profile->id.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 3, message.c_str(), -1, SQLITE_STATIC);

  if (sqlite3_step(stmt) != SQLITE_DONE) {
    LOGJ_DEBUG(logger, "[supervisor] insert activity step error",
               sqlite3_errmsg(db));
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return BadRequest(callback, "Failed to create activity");
  }
  sqlite3_finalize(stmt);

  const auto last_id = std::to_string(sqlite3_last_insert_rowid(db));
  sqlite3_close(db);

  Row activity{{"id", last_id},
               {"kind", "message"},
               {"body", message},
               {"created_at", "ahora"},
               {"profile_name", profile->name}};

  drogon::HttpViewData data;
  data.insert("ticket_id", ticket_id);
  data.insert("activity", activity);
  callback(drogon::HttpResponse::newHttpViewResponse(
      "supervisor_ticket_activity_message", data));
}

void Supervisor::TicketActivityAttachmentCreatePost(
    const drogon::HttpRequestPtr &req, Callback &&callback,
    const std::string &ticket_id, const std::string &activity_id) {
  auto *logger = quill::Frontend::get_logger("root");
  const auto token = req->getCookie("token");

  if (req->contentType() != drogon::CT_MULTIPART_FORM_DATA)
    return BadRequest(callback, "Content-Type must be multipart/form-data");

  drogon::MultiPartParser parser;
  if (parser.parse(req) != 0)
    return BadRequest(callback, "Invalid multipart form-data");

  const auto &files = parser.getFiles();
  if (files.empty())
    return BadRequest(callback, "Missing file");

  std::size_t upload_total = 0;
  for (const auto &f : files)
    upload_total += f.fileLength();
  if (upload_total > MaxActivityAttachmentSize)
    return BadRequest(callback, "Files exceed 25 MB per activity limit",
                      drogon::k413RequestEntityTooLarge);

  sqlite3 *db = ConnectDB();
  if (!db)
    return BadRequest(callback, "Database unavailable",
                      drogon::k503ServiceUnavailable);

  const auto profile = FetchProfile(logger, db, token);
  if (!profile) {
    sqlite3_close(db);
    return BadRequest(callback, "Forbidden", drogon::k403Forbidden);
  }

  if (!TicketActivityExists(logger, db, ticket_id, activity_id)) {
    sqlite3_close(db);
    return BadRequest(callback, "Ticket activity not found",
                      drogon::k404NotFound);
  }

  if (FetchActivityAttachmentTotal(db, activity_id) + upload_total >
      MaxActivityAttachmentSize) {
    sqlite3_close(db);
    return BadRequest(callback, "Activity attachments would exceed 25 MB",
                      drogon::k413RequestEntityTooLarge);
  }

  if (FetchTicketAttachmentTotal(db, ticket_id) + upload_total >
      MaxTicketAttachmentSize) {
    sqlite3_close(db);
    return BadRequest(callback, "Ticket attachments would exceed 75 MB",
                      drogon::k413RequestEntityTooLarge);
  }

  std::string html;
  for (const auto &upload_file : files) {
    const std::string fname =
        std::filesystem::path(upload_file.getFileName()).filename().string();
    if (fname.empty())
      continue;
    const auto fsize = upload_file.fileLength();
    const std::string mime = InferMimeType(fname);

    if (!Exec(db, "BEGIN IMMEDIATE"))
      continue;

    sqlite3_stmt *insert = nullptr;
    if (sqlite3_prepare_v2(db,
                           "INSERT INTO helpdesk_ticket_activity_attachment"
                           " (ticket_activity_id, file_path, file_name,"
                           "  file_size, mime_type)"
                           " VALUES (?, '', ?, ?, ?)",
                           -1, &insert, nullptr) != SQLITE_OK) {
      Exec(db, "ROLLBACK");
      continue;
    }
    sqlite3_bind_text(insert, 1, activity_id.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(insert, 2, fname.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int64(insert, 3, static_cast<sqlite3_int64>(fsize));
    sqlite3_bind_text(insert, 4, mime.c_str(), -1, SQLITE_STATIC);
    if (sqlite3_step(insert) != SQLITE_DONE) {
      sqlite3_finalize(insert);
      Exec(db, "ROLLBACK");
      continue;
    }
    sqlite3_finalize(insert);

    const auto att_id = std::to_string(sqlite3_last_insert_rowid(db));
    const std::filesystem::path rel =
        std::filesystem::path("role") / ("ticket_id=" + ticket_id) /
        ("activity_id=" + activity_id) / ("attachment_id=" + att_id);
    const auto abs_path = std::filesystem::current_path() /
                          ticketeer::core::conf::settings.UPLOAD_DIR / rel /
                          fname;

    if (!WriteFile(abs_path, upload_file.fileContent())) {
      Exec(db, "ROLLBACK");
      continue;
    }

    sqlite3_stmt *upd = nullptr;
    const std::string rel_str = rel.generic_string();
    if (sqlite3_prepare_v2(
            db,
            "UPDATE helpdesk_ticket_activity_attachment SET file_path = ?"
            " WHERE id = ?",
            -1, &upd, nullptr) != SQLITE_OK ||
        [&] {
          sqlite3_bind_text(upd, 1, rel_str.c_str(), -1, SQLITE_STATIC);
          sqlite3_bind_text(upd, 2, att_id.c_str(), -1, SQLITE_STATIC);
          return sqlite3_step(upd) != SQLITE_DONE;
        }()) {
      if (upd)
        sqlite3_finalize(upd);
      std::error_code ec;
      std::filesystem::remove(abs_path, ec);
      Exec(db, "ROLLBACK");
      continue;
    }
    sqlite3_finalize(upd);

    if (!Exec(db, "COMMIT")) {
      std::error_code ec;
      std::filesystem::remove(abs_path, ec);
      continue;
    }

    html +=
        "<a class=\"badge badge-outline gap-1\" href=\"/ticketeer/supervisor"
        "/ticket/" +
        ticket_id + "/activity/" + activity_id + "/attachment/" + att_id +
        "/download\">" +
        std::string(drogon::HttpViewData::htmlTranslate(fname)) + "</a>\n";
  }
  sqlite3_close(db);

  auto resp = drogon::HttpResponse::newHttpResponse();
  resp->setContentTypeCode(drogon::CT_NONE);
  resp->addHeader("Content-Type", "text/html; charset=utf-8");
  resp->setBody(html);
  callback(resp);
}

void Supervisor::TicketActivityAttachmentDownloadGet(
    const drogon::HttpRequestPtr &req, Callback &&callback,
    const std::string &ticket_id, const std::string &activity_id,
    const std::string &attachment_id) {
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

  auto file = FetchTicketActivityAttachmentFile(logger, db, ticket_id,
                                                activity_id, attachment_id);
  sqlite3_close(db);

  if (!file)
    return BadRequest(callback, "Attachment not found", drogon::k404NotFound);

  const auto abs_path =
      AttachmentAbsolutePath(file->file_path, file->file_name);
  if (!std::filesystem::is_regular_file(abs_path))
    return BadRequest(callback, "Attachment file not found",
                      drogon::k404NotFound);

  callback(drogon::HttpResponse::newFileResponse(
      abs_path.string(), file->file_name, drogon::CT_CUSTOM, file->mime_type,
      req));
}

} // namespace ticketeer
