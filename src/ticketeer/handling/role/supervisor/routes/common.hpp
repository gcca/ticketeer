#pragma once

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <drogon/DrTemplateBase.h>
#include <drogon/HttpController.h>
#include <json/json.h>
#include <quill/Frontend.h>
#include <quill/LogMacros.h>
#include <sqlite3.h>

#include "ticketeer/core/conf.hpp"

namespace ticketeer::handling::role::supervisor::routes::common {

using Callback = std::function<void(const drogon::HttpResponsePtr &)>;
using Row = std::map<std::string, std::string>;
using Rows = std::vector<Row>;

struct Profile {
  std::string id;
  std::string username;
  std::string name;
};

struct TicketCreateInput {
  std::string priority_id;
  std::string body;
};

struct TicketFilters {
  std::string search;
  std::string requester_id;
  std::string status_id;
  std::string priority_id;
  std::string assigned_to_id;
  std::string sort;
  std::string sort_dir;
};

struct AttachmentFile {
  std::string file_path;
  std::string file_name;
  std::string mime_type;
};

struct TicketStatusInput {
  std::string name;
  std::string display_name;
  std::string trait;
};

struct TicketPriorityInput {
  std::string name;
  std::string display_name;
};

inline constexpr std::size_t MaxActivityAttachmentSize = 25UL * 1024 * 1024;
inline constexpr std::size_t MaxTicketAttachmentSize = 75UL * 1024 * 1024;
inline constexpr int TicketPageSize = 9;

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

[[nodiscard]] inline int ParsePage(const std::string &value) {
  if (value.empty())
    return 1;
  int page = 0;
  for (const char ch : value) {
    if (ch < '0' || ch > '9')
      return 1;
    if (page > 100000)
      return 1;
    page = page * 10 + (ch - '0');
    if (page > 1000000)
      return 1;
  }
  return page > 0 ? page : 1;
}

[[nodiscard]] inline int PageCount(const int total_count) {
  return total_count > 0 ? (total_count + TicketPageSize - 1) / TicketPageSize
                         : 1;
}

[[nodiscard]] inline int ClampPage(const int page, const int page_count) {
  if (page < 1)
    return 1;
  if (page > page_count)
    return page_count;
  return page;
}

[[nodiscard]] inline std::string NormalizeTicketSort(std::string_view sort) {
  if (sort == "body" || sort == "requester" || sort == "status" ||
      sort == "priority" || sort == "assigned_to" || sort == "created_at" ||
      sort == "updated_at")
    return std::string(sort);
  return "created_at";
}

[[nodiscard]] inline std::string NormalizeSortDir(std::string_view dir) {
  return dir == "asc" ? "asc" : "desc";
}

[[nodiscard]] inline bool IsLeapYear(int year) {
  return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
}

[[nodiscard]] inline int DaysInMonth(int year, int month) {
  switch (month) {
  case 2:
    return IsLeapYear(year) ? 29 : 28;
  case 4:
  case 6:
  case 9:
  case 11:
    return 30;
  default:
    return 31;
  }
}

[[nodiscard]] inline bool IsValidIsoDate(std::string_view value) {
  if (value.empty())
    return true;
  if (value.size() != 10 || value[4] != '-' || value[7] != '-')
    return false;
  for (std::size_t i = 0; i < value.size(); ++i) {
    if (i == 4 || i == 7)
      continue;
    if (value[i] < '0' || value[i] > '9')
      return false;
  }
  const int year = (value[0] - '0') * 1000 + (value[1] - '0') * 100 +
                   (value[2] - '0') * 10 + (value[3] - '0');
  const int month = (value[5] - '0') * 10 + (value[6] - '0');
  const int day = (value[8] - '0') * 10 + (value[9] - '0');
  if (year < 1 || month < 1 || month > 12 || day < 1)
    return false;
  return day <= DaysInMonth(year, month);
}

[[nodiscard]] inline const char *TicketSortExpression(const std::string &sort) {
  if (sort == "body")
    return "t.body COLLATE NOCASE";
  if (sort == "requester")
    return "ru.name COLLATE NOCASE";
  if (sort == "status")
    return "ts.display_name COLLATE NOCASE";
  if (sort == "priority")
    return "p.display_name COLLATE NOCASE";
  if (sort == "assigned_to")
    return "COALESCE(au.name, '') COLLATE NOCASE";
  if (sort == "updated_at")
    return "t.updated_at";
  return "t.created_at";
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

[[nodiscard]] inline Rows FetchTicketRequesters(quill::Logger *logger,
                                                sqlite3 *db) {
  return FetchLookupRows(logger, db,
                         "SELECT DISTINCT rp.id, ru.name || ' (' || "
                         "ru.username || ')' "
                         "FROM helpdesk_ticket t "
                         "JOIN helpdesk_profile rp ON rp.id = t.requester_id "
                         "JOIN auth_user ru ON ru.id = rp.user_id "
                         "ORDER BY ru.name, ru.username",
                         "[supervisor] requester filter query error");
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
FetchDefaultTicketPriorityId(quill::Logger *logger, sqlite3 *db) {
  sqlite3_stmt *stmt = nullptr;
  if (sqlite3_prepare_v2(db,
                         "SELECT default_ticket_priority_id FROM "
                         "helpdesk_setting WHERE name = 'default'",
                         -1, &stmt, nullptr) != SQLITE_OK) {
    LOGJ_DEBUG(logger, "[supervisor] default priority query error",
               sqlite3_errmsg(db));
    return std::nullopt;
  }

  std::optional<std::string> id;
  if (sqlite3_step(stmt) == SQLITE_ROW)
    id = ColumnText(stmt, 0);

  sqlite3_finalize(stmt);
  return id;
}

[[nodiscard]] inline int FetchTicketBodyMaxlength(quill::Logger *logger,
                                                  sqlite3 *db) {
  sqlite3_stmt *stmt = nullptr;
  if (sqlite3_prepare_v2(db,
                         "SELECT ticket_body_maxlength FROM helpdesk_setting "
                         "WHERE name = 'default'",
                         -1, &stmt, nullptr) != SQLITE_OK) {
    LOGJ_DEBUG(logger, "[supervisor] ticket body maxlength query error",
               sqlite3_errmsg(db));
    return 570;
  }
  int maxlength = 570;
  if (sqlite3_step(stmt) == SQLITE_ROW)
    maxlength = sqlite3_column_int(stmt, 0);
  sqlite3_finalize(stmt);
  return maxlength;
}

[[nodiscard]] inline int FetchTicketActivityBodyMaxlength(quill::Logger *logger,
                                                          sqlite3 *db) {
  sqlite3_stmt *stmt = nullptr;
  if (sqlite3_prepare_v2(db,
                         "SELECT ticket_activity_body_maxlength FROM "
                         "helpdesk_setting WHERE name = 'default'",
                         -1, &stmt, nullptr) != SQLITE_OK) {
    LOGJ_DEBUG(logger, "[supervisor] activity body maxlength query error",
               sqlite3_errmsg(db));
    return 170;
  }
  int maxlength = 170;
  if (sqlite3_step(stmt) == SQLITE_ROW)
    maxlength = sqlite3_column_int(stmt, 0);
  sqlite3_finalize(stmt);
  return maxlength;
}

[[nodiscard]] inline int FetchTicketDueDelta(quill::Logger *logger,
                                             sqlite3 *db) {
  sqlite3_stmt *stmt = nullptr;
  if (sqlite3_prepare_v2(db,
                         "SELECT ticket_due_delta FROM helpdesk_setting "
                         "WHERE name = 'default'",
                         -1, &stmt, nullptr) != SQLITE_OK) {
    LOGJ_DEBUG(logger, "[supervisor] ticket due delta query error",
               sqlite3_errmsg(db));
    return 172800;
  }
  int delta = 172800;
  if (sqlite3_step(stmt) == SQLITE_ROW)
    delta = sqlite3_column_int(stmt, 0);
  sqlite3_finalize(stmt);
  return delta;
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

inline void AppendTicketFilterWhere(std::string &sql,
                                    std::vector<std::string> &values,
                                    const TicketFilters &filters) {
  std::vector<std::string> where;
  if (!filters.search.empty()) {
    where.push_back("t.body LIKE ?");
    values.push_back("%" + filters.search + "%");
  }
  if (!filters.requester_id.empty()) {
    where.push_back("t.requester_id = ?");
    values.push_back(filters.requester_id);
  }
  if (!filters.status_id.empty()) {
    where.push_back("t.status_id = ?");
    values.push_back(filters.status_id);
  }
  if (!filters.priority_id.empty()) {
    where.push_back("t.priority_id = ?");
    values.push_back(filters.priority_id);
  }
  if (filters.assigned_to_id == "none") {
    where.push_back("t.assigned_to_id IS NULL");
  } else if (!filters.assigned_to_id.empty()) {
    where.push_back("t.assigned_to_id = ?");
    values.push_back(filters.assigned_to_id);
  }

  if (where.empty())
    return;

  sql += " WHERE ";
  for (std::size_t i = 0; i < where.size(); ++i) {
    if (i > 0)
      sql += " AND ";
    sql += where[i];
  }
}

[[nodiscard]] inline Rows FetchTickets(quill::Logger *logger, sqlite3 *db,
                                       const TicketFilters &filters,
                                       const int page) {
  sqlite3_stmt *stmt = nullptr;
  std::string sql = R"SQL(
SELECT t.id, t.body, ts.display_name, p.display_name,
       ru.name, COALESCE(au.name, ''),
       strftime('%Y-%m-%d %H:%M:%S', datetime(t.created_at, '-5 hours')),
       strftime('%Y-%m-%d %H:%M:%S', datetime(t.updated_at, '-5 hours')),
       COALESCE(t.due_date, '')
  FROM helpdesk_ticket t
  JOIN helpdesk_ticket_status ts ON ts.id = t.status_id
  JOIN helpdesk_priority p       ON p.id  = t.priority_id
  JOIN helpdesk_profile rp       ON rp.id = t.requester_id
  JOIN auth_user ru              ON ru.id = rp.user_id
  LEFT JOIN helpdesk_profile ap  ON ap.id = t.assigned_to_id
  LEFT JOIN auth_user au         ON au.id = ap.user_id
)SQL";

  std::vector<std::string> values;
  AppendTicketFilterWhere(sql, values, filters);

  sql += " ORDER BY ";
  sql += TicketSortExpression(filters.sort);
  sql += filters.sort_dir == "asc" ? " ASC" : " DESC";
  sql += ", t.id DESC";
  sql += " LIMIT ? OFFSET ?";

  if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
    LOGJ_DEBUG(logger, "[supervisor] ticket list query error",
               sqlite3_errmsg(db));
    return {};
  }

  int bind_idx = 1;
  for (const auto &value : values)
    sqlite3_bind_text(stmt, bind_idx++, value.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_int(stmt, bind_idx++, TicketPageSize);
  sqlite3_bind_int(stmt, bind_idx++, (page - 1) * TicketPageSize);

  Rows tickets;
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    tickets.push_back({{"id", ColumnText(stmt, 0)},
                       {"body", ColumnText(stmt, 1)},
                       {"status_name", ColumnText(stmt, 2)},
                       {"priority_name", ColumnText(stmt, 3)},
                       {"requester_name", ColumnText(stmt, 4)},
                       {"assigned_to_name", ColumnText(stmt, 5)},
                       {"created_at", ColumnText(stmt, 6)},
                       {"updated_at", ColumnText(stmt, 7)},
                       {"due_date", ColumnText(stmt, 8)}});
  }

  sqlite3_finalize(stmt);
  return tickets;
}

[[nodiscard]] inline int CountTickets(quill::Logger *logger, sqlite3 *db,
                                      const TicketFilters &filters) {
  sqlite3_stmt *stmt = nullptr;
  std::string sql = R"SQL(
SELECT COUNT(*)
  FROM helpdesk_ticket t
  JOIN helpdesk_ticket_status ts ON ts.id = t.status_id
  JOIN helpdesk_priority p       ON p.id  = t.priority_id
  JOIN helpdesk_profile rp       ON rp.id = t.requester_id
  JOIN auth_user ru              ON ru.id = rp.user_id
  LEFT JOIN helpdesk_profile ap  ON ap.id = t.assigned_to_id
  LEFT JOIN auth_user au         ON au.id = ap.user_id
)SQL";
  std::vector<std::string> values;
  AppendTicketFilterWhere(sql, values, filters);

  if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
    LOGJ_DEBUG(logger, "[supervisor] ticket count query error",
               sqlite3_errmsg(db));
    return 0;
  }

  int bind_idx = 1;
  for (const auto &value : values)
    sqlite3_bind_text(stmt, bind_idx++, value.c_str(), -1, SQLITE_STATIC);

  int total_count = 0;
  if (sqlite3_step(stmt) == SQLITE_ROW)
    total_count = sqlite3_column_int(stmt, 0);
  sqlite3_finalize(stmt);
  return total_count;
}

[[nodiscard]] inline drogon::HttpViewData
BuildTicketListData(quill::Logger *logger, sqlite3 *db,
                    const TicketFilters &filters, const int requested_page) {
  TicketFilters normalized_filters = filters;
  normalized_filters.sort = NormalizeTicketSort(filters.sort);
  normalized_filters.sort_dir = NormalizeSortDir(filters.sort_dir);

  const int total_count = CountTickets(logger, db, normalized_filters);
  const int page_count = PageCount(total_count);
  const int page = ClampPage(requested_page, page_count);
  auto tickets = FetchTickets(logger, db, normalized_filters, page);
  auto requesters = FetchTicketRequesters(logger, db);
  auto statuses = FetchStatuses(logger, db);
  auto priorities = FetchPriorities(logger, db);
  auto assignees = FetchAssignees(logger, db);

  drogon::HttpViewData data;
  data.insert("tickets", tickets);
  data.insert("search", normalized_filters.search);
  data.insert("requester_id", normalized_filters.requester_id);
  data.insert("status_id", normalized_filters.status_id);
  data.insert("priority_id", normalized_filters.priority_id);
  data.insert("assigned_to_id", normalized_filters.assigned_to_id);
  data.insert("sort", normalized_filters.sort);
  data.insert("dir", normalized_filters.sort_dir);
  data.insert("requesters", requesters);
  data.insert("statuses", statuses);
  data.insert("priorities", priorities);
  data.insert("assignees", assignees);
  data.insert("page", std::to_string(page));
  data.insert("page_count", std::to_string(page_count));
  data.insert("prev_page", std::to_string(page > 1 ? page - 1 : 1));
  data.insert("next_page",
              std::to_string(page < page_count ? page + 1 : page_count));
  data.insert("total_count", std::to_string(total_count));
  return data;
}

[[nodiscard]] inline std::optional<Row>
FetchTicketDetails(quill::Logger *logger, sqlite3 *db,
                   const std::string &ticket_id) {
  sqlite3_stmt *stmt = nullptr;
  const char *sql = R"SQL(
SELECT t.id, t.body, t.status_id, ts.display_name, p.display_name,
       strftime('%Y-%m-%d %H:%M:%S', datetime(t.created_at, '-5 hours')),
       COALESCE(t.due_date, ''),
       COALESCE(au.name, '') AS assigned_to_name,
       ru.name AS requester_name,
       ru.username AS requester_username,
       COALESCE(au.username, '') AS assigned_to_username,
       COALESCE(t.assigned_to_id, '') AS assigned_to_id,
       t.priority_id, ts.trait
  FROM helpdesk_ticket t
  JOIN helpdesk_ticket_status ts ON ts.id = t.status_id
  JOIN helpdesk_priority p       ON p.id  = t.priority_id
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
                 {"body", ColumnText(stmt, 1)},
                 {"status_id", ColumnText(stmt, 2)},
                 {"status_name", ColumnText(stmt, 3)},
                 {"priority_name", ColumnText(stmt, 4)},
                 {"created_at", ColumnText(stmt, 5)},
                 {"due_date", ColumnText(stmt, 6)},
                 {"assigned_to_name", ColumnText(stmt, 7)},
                 {"requester_name", ColumnText(stmt, 8)},
                 {"requester_username", ColumnText(stmt, 9)},
                 {"assigned_to_username", ColumnText(stmt, 10)},
                 {"assigned_to_id", ColumnText(stmt, 11)},
                 {"priority_id", ColumnText(stmt, 12)},
                 {"status_trait", ColumnText(stmt, 13)}};
  }

  sqlite3_finalize(stmt);
  return ticket;
}

[[nodiscard]] inline std::optional<std::string>
FetchTicketTrait(quill::Logger *logger, sqlite3 *db,
                 const std::string &ticket_id) {
  sqlite3_stmt *stmt = nullptr;
  const char *sql = R"SQL(
SELECT ts.trait
  FROM helpdesk_ticket t
  JOIN helpdesk_ticket_status ts ON ts.id = t.status_id
 WHERE t.id = ?
 LIMIT 1
)SQL";
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK)
    return std::nullopt;
  sqlite3_bind_text(stmt, 1, ticket_id.c_str(), -1, SQLITE_STATIC);
  std::optional<std::string> trait;
  if (sqlite3_step(stmt) == SQLITE_ROW)
    trait = ColumnText(stmt, 0);
  sqlite3_finalize(stmt);
  return trait;
}

[[nodiscard]] inline std::optional<std::string>
FetchStatusTrait(quill::Logger *logger, sqlite3 *db, const std::string &id) {
  sqlite3_stmt *stmt = nullptr;
  if (sqlite3_prepare_v2(db, "SELECT trait FROM helpdesk_ticket_status WHERE id = ?",
                         -1, &stmt, nullptr) != SQLITE_OK) {
    LOGJ_DEBUG(logger, "[supervisor] status trait query error", sqlite3_errmsg(db));
    return std::nullopt;
  }
  sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_STATIC);
  std::optional<std::string> trait;
  if (sqlite3_step(stmt) == SQLITE_ROW)
    trait = ColumnText(stmt, 0);
  sqlite3_finalize(stmt);
  return trait;
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

[[nodiscard]] inline bool UpdateTicketDueDate(quill::Logger *logger,
                                              sqlite3 *db,
                                              const std::string &ticket_id,
                                              const std::string &new_due_date) {
  sqlite3_stmt *stmt = nullptr;
  const char *sql = R"SQL(
UPDATE helpdesk_ticket
   SET due_date = NULLIF(?, '')
 WHERE id = ?
)SQL";

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    LOGJ_DEBUG(logger, "[supervisor] update due_date query error",
               sqlite3_errmsg(db));
    return false;
  }

  sqlite3_bind_text(stmt, 1, new_due_date.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 2, ticket_id.c_str(), -1, SQLITE_STATIC);
  const bool ok = sqlite3_step(stmt) == SQLITE_DONE && sqlite3_changes(db) == 1;
  sqlite3_finalize(stmt);
  return ok;
}

[[nodiscard]] inline Rows FetchActivities(quill::Logger *logger, sqlite3 *db,
                                          const std::string &ticket_id) {
  sqlite3_stmt *stmt = nullptr;
  const char *sql = R"SQL(
SELECT ta.id, ta.kind, ta.body,
       strftime('%Y-%m-%d %H:%M:%S', datetime(ta.created_at, '-5 hours')),
       u.name
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
    const auto &priority_id = (*body)["priority_id"];
    const auto &ticket_body = (*body)["body"];

    if (!priority_id.isString() || !ticket_body.isString())
      return std::nullopt;

    TicketCreateInput input{priority_id.asString(), ticket_body.asString()};
    if (input.priority_id.empty() || input.body.empty())
      return std::nullopt;
    return input;
  }

  TicketCreateInput input{req->getParameter("priority_id"),
                          req->getParameter("body")};
  if (input.priority_id.empty() || input.body.empty())
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

[[nodiscard]] inline std::string Trim(std::string value) {
  const auto not_space = [](unsigned char c) { return !std::isspace(c); };
  value.erase(value.begin(),
              std::find_if(value.begin(), value.end(), not_space));
  value.erase(std::find_if(value.rbegin(), value.rend(), not_space).base(),
              value.end());
  return value;
}

[[nodiscard]] inline std::optional<TicketStatusInput>
ParseTicketStatusInput(const drogon::HttpRequestPtr &req) {
  TicketStatusInput input{Trim(req->getParameter("name")),
                          Trim(req->getParameter("display_name")),
                          req->getParameter("trait")};
  if (input.name.empty() || input.display_name.empty() || input.trait.empty())
    return std::nullopt;
  return input;
}

[[nodiscard]] inline std::optional<TicketPriorityInput>
ParseTicketPriorityInput(const drogon::HttpRequestPtr &req) {
  TicketPriorityInput input{Trim(req->getParameter("name")),
                            Trim(req->getParameter("display_name"))};
  if (input.name.empty() || input.display_name.empty())
    return std::nullopt;
  return input;
}

[[nodiscard]] inline Rows FetchTicketStatusConfigRows(quill::Logger *logger,
                                                      sqlite3 *db) {
  sqlite3_stmt *stmt = nullptr;
  const char *sql = R"SQL(
SELECT ts.id, ts.name, ts.display_name, ts.trait,
       CAST((SELECT COUNT(*) FROM helpdesk_ticket t WHERE t.status_id = ts.id) AS TEXT),
       CAST((SELECT COUNT(*) FROM helpdesk_setting s
              WHERE s.default_status_id = ts.id OR s.assigned_status_id = ts.id) AS TEXT)
  FROM helpdesk_ticket_status ts
 ORDER BY ts.id
)SQL";
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    LOGJ_DEBUG(logger, "[supervisor] ticket status config list query error",
               sqlite3_errmsg(db));
    return {};
  }
  Rows rows;
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    rows.push_back({{"id", ColumnText(stmt, 0)},
                    {"name", ColumnText(stmt, 1)},
                    {"display_name", ColumnText(stmt, 2)},
                    {"trait", ColumnText(stmt, 3)},
                    {"ticket_count", ColumnText(stmt, 4)},
                    {"setting_count", ColumnText(stmt, 5)}});
  }
  sqlite3_finalize(stmt);
  return rows;
}

[[nodiscard]] inline std::optional<Row>
FetchTicketStatusConfigRow(quill::Logger *logger, sqlite3 *db,
                           const std::string &id) {
  sqlite3_stmt *stmt = nullptr;
  if (sqlite3_prepare_v2(db,
                         "SELECT id, name, display_name, trait"
                         "  FROM helpdesk_ticket_status WHERE id = ? LIMIT 1",
                         -1, &stmt, nullptr) != SQLITE_OK) {
    LOGJ_DEBUG(logger, "[supervisor] ticket status config row query error",
               sqlite3_errmsg(db));
    return std::nullopt;
  }
  sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_STATIC);
  std::optional<Row> row;
  if (sqlite3_step(stmt) == SQLITE_ROW)
    row = Row{{"id", ColumnText(stmt, 0)},
              {"name", ColumnText(stmt, 1)},
              {"display_name", ColumnText(stmt, 2)},
              {"trait", ColumnText(stmt, 3)}};
  sqlite3_finalize(stmt);
  return row;
}

[[nodiscard]] inline bool TicketStatusExists(quill::Logger *logger, sqlite3 *db,
                                             const std::string &id) {
  sqlite3_stmt *stmt = nullptr;
  if (sqlite3_prepare_v2(db,
                         "SELECT id FROM helpdesk_ticket_status WHERE id = ?",
                         -1, &stmt, nullptr) != SQLITE_OK) {
    LOGJ_DEBUG(logger, "[supervisor] ticket status exists query error",
               sqlite3_errmsg(db));
    return false;
  }
  sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_STATIC);
  const bool exists = sqlite3_step(stmt) == SQLITE_ROW;
  sqlite3_finalize(stmt);
  return exists;
}

[[nodiscard]] inline bool ReassignTicketStatus(quill::Logger *logger,
                                               sqlite3 *db,
                                               const std::string &from_id,
                                               const std::string &to_id) {
  sqlite3_stmt *stmt = nullptr;
  if (sqlite3_prepare_v2(
          db, "UPDATE helpdesk_ticket SET status_id = ? WHERE status_id = ?",
          -1, &stmt, nullptr) != SQLITE_OK) {
    LOGJ_DEBUG(logger,
               "[supervisor] ticket status reassign tickets query error",
               sqlite3_errmsg(db));
    return false;
  }
  sqlite3_bind_text(stmt, 1, to_id.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 2, from_id.c_str(), -1, SQLITE_STATIC);
  const bool t_ok = sqlite3_step(stmt) == SQLITE_DONE;
  sqlite3_finalize(stmt);
  if (!t_ok) {
    LOGJ_DEBUG(logger, "[supervisor] ticket status reassign tickets step error",
               sqlite3_errmsg(db));
    return false;
  }

  if (sqlite3_prepare_v2(db,
                         "UPDATE helpdesk_setting SET default_status_id = ?"
                         " WHERE default_status_id = ?",
                         -1, &stmt, nullptr) != SQLITE_OK) {
    LOGJ_DEBUG(logger,
               "[supervisor] ticket status reassign default_status query error",
               sqlite3_errmsg(db));
    return false;
  }
  sqlite3_bind_text(stmt, 1, to_id.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 2, from_id.c_str(), -1, SQLITE_STATIC);
  const bool s1_ok = sqlite3_step(stmt) == SQLITE_DONE;
  sqlite3_finalize(stmt);
  if (!s1_ok) {
    LOGJ_DEBUG(logger,
               "[supervisor] ticket status reassign default_status step error",
               sqlite3_errmsg(db));
    return false;
  }

  if (sqlite3_prepare_v2(db,
                         "UPDATE helpdesk_setting SET assigned_status_id = ?"
                         " WHERE assigned_status_id = ?",
                         -1, &stmt, nullptr) != SQLITE_OK) {
    LOGJ_DEBUG(
        logger,
        "[supervisor] ticket status reassign assigned_status query error",
        sqlite3_errmsg(db));
    return false;
  }
  sqlite3_bind_text(stmt, 1, to_id.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 2, from_id.c_str(), -1, SQLITE_STATIC);
  const bool s2_ok = sqlite3_step(stmt) == SQLITE_DONE;
  sqlite3_finalize(stmt);
  if (!s2_ok)
    LOGJ_DEBUG(logger,
               "[supervisor] ticket status reassign assigned_status step error",
               sqlite3_errmsg(db));
  return s2_ok;
}

[[nodiscard]] inline bool InsertTicketStatus(quill::Logger *logger, sqlite3 *db,
                                             const TicketStatusInput &input) {
  sqlite3_stmt *stmt = nullptr;
  const char *sql = R"SQL(
INSERT INTO helpdesk_ticket_status (name, display_name, trait)
VALUES (?, ?, ?)
)SQL";
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    LOGJ_DEBUG(logger, "[supervisor] ticket status insert query error",
               sqlite3_errmsg(db));
    return false;
  }
  sqlite3_bind_text(stmt, 1, input.name.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 2, input.display_name.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 3, input.trait.c_str(), -1, SQLITE_STATIC);
  const bool ok = sqlite3_step(stmt) == SQLITE_DONE;
  if (!ok)
    LOGJ_DEBUG(logger, "[supervisor] ticket status insert step error",
               sqlite3_errmsg(db));
  sqlite3_finalize(stmt);
  return ok;
}

[[nodiscard]] inline bool
UpdateTicketStatusRecord(quill::Logger *logger, sqlite3 *db,
                         const std::string &id,
                         const TicketStatusInput &input) {
  sqlite3_stmt *stmt = nullptr;
  const char *sql = R"SQL(
UPDATE helpdesk_ticket_status
   SET name = ?,
       display_name = ?,
       trait = ?
 WHERE id = ?
)SQL";
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    LOGJ_DEBUG(logger, "[supervisor] ticket status update query error",
               sqlite3_errmsg(db));
    return false;
  }
  sqlite3_bind_text(stmt, 1, input.name.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 2, input.display_name.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 3, input.trait.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 4, id.c_str(), -1, SQLITE_STATIC);
  const bool done = sqlite3_step(stmt) == SQLITE_DONE;
  if (!done)
    LOGJ_DEBUG(logger, "[supervisor] ticket status update step error",
               sqlite3_errmsg(db));
  const int changes = sqlite3_changes(db);
  sqlite3_finalize(stmt);
  return done && changes == 1;
}

[[nodiscard]] inline bool DeleteTicketStatus(quill::Logger *logger, sqlite3 *db,
                                             const std::string &id) {
  sqlite3_stmt *stmt = nullptr;
  if (sqlite3_prepare_v2(db, "DELETE FROM helpdesk_ticket_status WHERE id = ?",
                         -1, &stmt, nullptr) != SQLITE_OK) {
    LOGJ_DEBUG(logger, "[supervisor] ticket status delete query error",
               sqlite3_errmsg(db));
    return false;
  }
  sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_STATIC);
  const bool ok = sqlite3_step(stmt) == SQLITE_DONE && sqlite3_changes(db) == 1;
  if (!ok)
    LOGJ_DEBUG(logger, "[supervisor] ticket status delete step error",
               sqlite3_errmsg(db));
  sqlite3_finalize(stmt);
  return ok;
}

[[nodiscard]] inline Rows FetchTicketPriorityConfigRows(quill::Logger *logger,
                                                        sqlite3 *db) {
  sqlite3_stmt *stmt = nullptr;
  const char *sql = R"SQL(
SELECT p.id, p.name, p.display_name,
       CAST((SELECT COUNT(*) FROM helpdesk_ticket t WHERE t.priority_id = p.id) AS TEXT),
       CAST((SELECT COUNT(*) FROM helpdesk_setting s
              WHERE s.default_ticket_priority_id = p.id) AS TEXT)
  FROM helpdesk_priority p
 ORDER BY p.id
)SQL";
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    LOGJ_DEBUG(logger, "[supervisor] ticket priority config list query error",
               sqlite3_errmsg(db));
    return {};
  }
  Rows rows;
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    rows.push_back({{"id", ColumnText(stmt, 0)},
                    {"name", ColumnText(stmt, 1)},
                    {"display_name", ColumnText(stmt, 2)},
                    {"ticket_count", ColumnText(stmt, 3)},
                    {"setting_count", ColumnText(stmt, 4)}});
  }
  sqlite3_finalize(stmt);
  return rows;
}

[[nodiscard]] inline std::optional<Row>
FetchTicketPriorityConfigRow(quill::Logger *logger, sqlite3 *db,
                             const std::string &id) {
  sqlite3_stmt *stmt = nullptr;
  if (sqlite3_prepare_v2(db,
                         "SELECT id, name, display_name"
                         "  FROM helpdesk_priority WHERE id = ? LIMIT 1",
                         -1, &stmt, nullptr) != SQLITE_OK) {
    LOGJ_DEBUG(logger, "[supervisor] ticket priority config row query error",
               sqlite3_errmsg(db));
    return std::nullopt;
  }
  sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_STATIC);
  std::optional<Row> row;
  if (sqlite3_step(stmt) == SQLITE_ROW)
    row = Row{{"id", ColumnText(stmt, 0)},
              {"name", ColumnText(stmt, 1)},
              {"display_name", ColumnText(stmt, 2)}};
  sqlite3_finalize(stmt);
  return row;
}

[[nodiscard]] inline bool TicketPriorityExists(quill::Logger *logger, sqlite3 *db,
                                               const std::string &id) {
  sqlite3_stmt *stmt = nullptr;
  if (sqlite3_prepare_v2(db,
                         "SELECT id FROM helpdesk_priority WHERE id = ?",
                         -1, &stmt, nullptr) != SQLITE_OK) {
    LOGJ_DEBUG(logger, "[supervisor] ticket priority exists query error",
               sqlite3_errmsg(db));
    return false;
  }
  sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_STATIC);
  const bool exists = sqlite3_step(stmt) == SQLITE_ROW;
  sqlite3_finalize(stmt);
  return exists;
}

[[nodiscard]] inline bool ReassignTicketPriority(quill::Logger *logger,
                                                 sqlite3 *db,
                                                 const std::string &from_id,
                                                 const std::string &to_id) {
  sqlite3_stmt *stmt = nullptr;
  if (sqlite3_prepare_v2(
          db, "UPDATE helpdesk_ticket SET priority_id = ? WHERE priority_id = ?",
          -1, &stmt, nullptr) != SQLITE_OK) {
    LOGJ_DEBUG(logger,
               "[supervisor] ticket priority reassign query error",
               sqlite3_errmsg(db));
    return false;
  }
  sqlite3_bind_text(stmt, 1, to_id.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 2, from_id.c_str(), -1, SQLITE_STATIC);
  const bool ok = sqlite3_step(stmt) == SQLITE_DONE;
  sqlite3_finalize(stmt);
  if (!ok) {
    LOGJ_DEBUG(logger, "[supervisor] ticket priority reassign step error",
               sqlite3_errmsg(db));
    return false;
  }

  sqlite3_stmt *setting_stmt = nullptr;
  if (sqlite3_prepare_v2(db,
                         "UPDATE helpdesk_setting "
                         "SET default_ticket_priority_id = ? "
                         "WHERE default_ticket_priority_id = ?",
                         -1, &setting_stmt, nullptr) != SQLITE_OK) {
    LOGJ_DEBUG(logger,
               "[supervisor] setting priority reassign query error",
               sqlite3_errmsg(db));
    return false;
  }
  sqlite3_bind_text(setting_stmt, 1, to_id.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(setting_stmt, 2, from_id.c_str(), -1, SQLITE_STATIC);
  const bool setting_ok = sqlite3_step(setting_stmt) == SQLITE_DONE;
  sqlite3_finalize(setting_stmt);
  if (!setting_ok)
    LOGJ_DEBUG(logger, "[supervisor] setting priority reassign step error",
               sqlite3_errmsg(db));
  return setting_ok;
}

[[nodiscard]] inline bool InsertTicketPriority(quill::Logger *logger, sqlite3 *db,
                                               const TicketPriorityInput &input) {
  sqlite3_stmt *stmt = nullptr;
  const char *sql = R"SQL(
INSERT INTO helpdesk_priority (name, display_name)
VALUES (?, ?)
)SQL";
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    LOGJ_DEBUG(logger, "[supervisor] ticket priority insert query error",
               sqlite3_errmsg(db));
    return false;
  }
  sqlite3_bind_text(stmt, 1, input.name.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 2, input.display_name.c_str(), -1, SQLITE_STATIC);
  const bool ok = sqlite3_step(stmt) == SQLITE_DONE;
  if (!ok)
    LOGJ_DEBUG(logger, "[supervisor] ticket priority insert step error",
               sqlite3_errmsg(db));
  sqlite3_finalize(stmt);
  return ok;
}

[[nodiscard]] inline bool
UpdateTicketPriorityRecord(quill::Logger *logger, sqlite3 *db,
                           const std::string &id,
                           const TicketPriorityInput &input) {
  sqlite3_stmt *stmt = nullptr;
  const char *sql = R"SQL(
UPDATE helpdesk_priority
   SET name = ?,
       display_name = ?
 WHERE id = ?
)SQL";
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    LOGJ_DEBUG(logger, "[supervisor] ticket priority update query error",
               sqlite3_errmsg(db));
    return false;
  }
  sqlite3_bind_text(stmt, 1, input.name.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 2, input.display_name.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 3, id.c_str(), -1, SQLITE_STATIC);
  const bool done = sqlite3_step(stmt) == SQLITE_DONE;
  if (!done)
    LOGJ_DEBUG(logger, "[supervisor] ticket priority update step error",
               sqlite3_errmsg(db));
  const int changes = sqlite3_changes(db);
  sqlite3_finalize(stmt);
  return done && changes == 1;
}

[[nodiscard]] inline bool DeleteTicketPriority(quill::Logger *logger, sqlite3 *db,
                                               const std::string &id) {
  sqlite3_stmt *stmt = nullptr;
  if (sqlite3_prepare_v2(db, "DELETE FROM helpdesk_priority WHERE id = ?",
                         -1, &stmt, nullptr) != SQLITE_OK) {
    LOGJ_DEBUG(logger, "[supervisor] ticket priority delete query error",
               sqlite3_errmsg(db));
    return false;
  }
  sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_STATIC);
  const bool ok = sqlite3_step(stmt) == SQLITE_DONE && sqlite3_changes(db) == 1;
  if (!ok)
    LOGJ_DEBUG(logger, "[supervisor] ticket priority delete step error",
               sqlite3_errmsg(db));
  sqlite3_finalize(stmt);
  return ok;
}

[[nodiscard]] inline std::optional<Row>
FetchDefaultSetting(quill::Logger *logger, sqlite3 *db) {
  sqlite3_stmt *stmt = nullptr;
  if (sqlite3_prepare_v2(db,
                         "SELECT default_status_id, default_assigned_to_id, "
                         "default_ticket_priority_id, assigned_status_id, "
                         "system_profile_id, "
                         "ticket_body_maxlength, "
                         "ticket_activity_body_maxlength, ticket_due_delta "
                         "FROM helpdesk_setting WHERE name = 'default' LIMIT 1",
                         -1, &stmt, nullptr) != SQLITE_OK) {
    LOGJ_DEBUG(logger, "[supervisor] settings query error", sqlite3_errmsg(db));
    return std::nullopt;
  }
  std::optional<Row> row;
  if (sqlite3_step(stmt) == SQLITE_ROW)
    row = Row{{"default_status_id", ColumnText(stmt, 0)},
              {"default_assigned_to_id", ColumnText(stmt, 1)},
              {"default_ticket_priority_id", ColumnText(stmt, 2)},
              {"assigned_status_id", ColumnText(stmt, 3)},
              {"system_profile_id", ColumnText(stmt, 4)},
              {"ticket_body_maxlength", ColumnText(stmt, 5)},
              {"ticket_activity_body_maxlength", ColumnText(stmt, 6)},
              {"ticket_due_delta", ColumnText(stmt, 7)}};
  sqlite3_finalize(stmt);
  return row;
}

[[nodiscard]] inline Rows FetchSystemProfiles(quill::Logger *logger,
                                              sqlite3 *db) {
  return FetchLookupRows(logger, db,
                         "SELECT p.id, u.name || ' (' || u.username || ')' "
                         "FROM helpdesk_profile p "
                         "JOIN auth_user u ON u.id = p.user_id "
                         "WHERE p.role NOT IN ('requester') "
                         "ORDER BY p.role, u.name",
                         "[supervisor] system profiles list query error");
}

[[nodiscard]] inline std::optional<int>
ParsePositiveInt(const std::string &value) {
  if (value.empty())
    return std::nullopt;
  for (unsigned char c : value) {
    if (!std::isdigit(c))
      return std::nullopt;
  }
  try {
    const long parsed = std::stol(value);
    if (parsed <= 0)
      return std::nullopt;
    return static_cast<int>(parsed);
  } catch (...) {
    return std::nullopt;
  }
}

[[nodiscard]] inline bool
UpsertDefaultSetting(quill::Logger *logger, sqlite3 *db,
                     const std::string &default_status_id,
                     const std::string &default_assigned_to_id,
                     const std::string &default_ticket_priority_id,
                     const std::string &assigned_status_id,
                     const std::string &system_profile_id,
                     int ticket_body_maxlength,
                     int ticket_activity_body_maxlength,
                     int ticket_due_delta) {
  sqlite3_stmt *stmt = nullptr;
  const char *sql = R"SQL(
INSERT OR REPLACE INTO helpdesk_setting
  (name, default_status_id, default_assigned_to_id,
   default_ticket_priority_id, assigned_status_id, system_profile_id,
   ticket_body_maxlength, ticket_activity_body_maxlength, ticket_due_delta)
VALUES ('default', ?, ?, ?, ?, ?, ?, ?, ?)
)SQL";
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    LOGJ_DEBUG(logger, "[supervisor] settings upsert query error",
               sqlite3_errmsg(db));
    return false;
  }
  sqlite3_bind_text(stmt, 1, default_status_id.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 2, default_assigned_to_id.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 3, default_ticket_priority_id.c_str(), -1,
                    SQLITE_STATIC);
  sqlite3_bind_text(stmt, 4, assigned_status_id.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 5, system_profile_id.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_int(stmt, 6, ticket_body_maxlength);
  sqlite3_bind_int(stmt, 7, ticket_activity_body_maxlength);
  sqlite3_bind_int(stmt, 8, ticket_due_delta);
  const bool ok = sqlite3_step(stmt) == SQLITE_DONE;
  if (!ok)
    LOGJ_DEBUG(logger, "[supervisor] settings upsert step error",
               sqlite3_errmsg(db));
  sqlite3_finalize(stmt);
  return ok;
}

inline void RenderSettingsConfig(const Callback &callback,
                                 quill::Logger *logger, sqlite3 *db) {
  auto setting = FetchDefaultSetting(logger, db);
  auto statuses = FetchStatuses(logger, db);
  auto assignees = FetchAssignees(logger, db);
  auto priorities = FetchPriorities(logger, db);
  auto profiles = FetchSystemProfiles(logger, db);

  const std::string empty;
  drogon::HttpViewData data;
  data.insert("default_status_id",
              setting ? setting->at("default_status_id") : empty);
  data.insert("assigned_status_id",
              setting ? setting->at("assigned_status_id") : empty);
  data.insert("default_assigned_to_id",
              setting ? setting->at("default_assigned_to_id") : empty);
  data.insert("default_ticket_priority_id",
              setting ? setting->at("default_ticket_priority_id") : empty);
  data.insert("system_profile_id",
              setting ? setting->at("system_profile_id") : empty);
  data.insert("ticket_body_maxlength",
               setting ? setting->at("ticket_body_maxlength") : "570");
  data.insert("ticket_activity_body_maxlength",
               setting ? setting->at("ticket_activity_body_maxlength") : "170");
  data.insert("ticket_due_delta",
              setting ? setting->at("ticket_due_delta") : "172800");
  data.insert("statuses", statuses);
  data.insert("assignees", assignees);
  data.insert("priorities", priorities);
  data.insert("profiles", profiles);
  callback(drogon::HttpResponse::newHttpViewResponse(
      "supervisor_config_settings", data));
}

[[nodiscard]] inline bool
RenderTicketStatusConfig(const Callback &callback, quill::Logger *logger,
                         sqlite3 *db, const std::string &editing_id) {
  auto ticket_statuses = FetchTicketStatusConfigRows(logger, db);

  std::string form_id;
  std::string form_name;
  std::string form_display_name;
  std::string form_trait;
  std::string form_post = "/ticketeer/supervisor/config/ticket-status/save";

  if (!editing_id.empty()) {
    const auto row = FetchTicketStatusConfigRow(logger, db, editing_id);
    if (!row)
      return false;
    form_id = row->at("id");
    form_name = row->at("name");
    form_display_name = row->at("display_name");
    form_trait = row->at("trait");
    form_post =
        "/ticketeer/supervisor/config/ticket-status/" + form_id + "/save";
  }

  drogon::HttpViewData data;
  data.insert("ticket_statuses", ticket_statuses);
  data.insert("form_id", form_id);
  data.insert("form_name", form_name);
  data.insert("form_display_name", form_display_name);
  data.insert("form_trait", form_trait);
  data.insert("form_post", form_post);
  callback(drogon::HttpResponse::newHttpViewResponse(
      "supervisor_config_ticket_status", data));
  return true;
}

[[nodiscard]] inline bool
RenderTicketPriorityConfig(const Callback &callback, quill::Logger *logger,
                           sqlite3 *db, const std::string &editing_id) {
  auto ticket_priorities = FetchTicketPriorityConfigRows(logger, db);

  std::string form_id;
  std::string form_name;
  std::string form_display_name;
  std::string form_post = "/ticketeer/supervisor/config/ticket-priority/save";

  if (!editing_id.empty()) {
    const auto row = FetchTicketPriorityConfigRow(logger, db, editing_id);
    if (!row)
      return false;
    form_id = row->at("id");
    form_name = row->at("name");
    form_display_name = row->at("display_name");
    form_post =
        "/ticketeer/supervisor/config/ticket-priority/" + form_id + "/save";
  }

  drogon::HttpViewData data;
  data.insert("ticket_priorities", ticket_priorities);
  data.insert("form_id", form_id);
  data.insert("form_name", form_name);
  data.insert("form_display_name", form_display_name);
  data.insert("form_post", form_post);
  callback(drogon::HttpResponse::newHttpViewResponse(
      "supervisor_config_ticket_priority", data));
  return true;
}

} // namespace ticketeer::handling::role::supervisor::routes::common
