#include "routes.hpp"

#include <filesystem>
#include <fstream>
#include <map>
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

inline constexpr int TicketPageSize = 9;

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
  std::string status_id;
  std::string priority_id;
  std::string sort;
  std::string sort_dir;
};

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
  if (sort == "body" || sort == "status" || sort == "priority" ||
      sort == "created_at" || sort == "updated_at")
    return std::string(sort);
  return "created_at";
}

[[nodiscard]] inline std::string NormalizeSortDir(std::string_view dir) {
  return dir == "asc" ? "asc" : "desc";
}

[[nodiscard]] inline const char *TicketSortExpression(const std::string &sort) {
  if (sort == "body")
    return "t.body COLLATE NOCASE";
  if (sort == "status")
    return "ts.display_name COLLATE NOCASE";
  if (sort == "priority")
    return "p.display_name COLLATE NOCASE";
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
    profile =
        Profile{ColumnText(stmt, 0), ColumnText(stmt, 1), ColumnText(stmt, 2)};
  else
    LOGJ_DEBUG(logger, "[requester] profile not found");

  sqlite3_finalize(stmt);
  return profile;
}

[[nodiscard]] inline Rows FetchLookupRows(quill::Logger *logger, sqlite3 *db,
                                          const char *sql,
                                          const char *log_message) {
  sqlite3_stmt *stmt = nullptr;
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    LOGJ_DEBUG(logger, "[requester] lookup query error", log_message,
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
                         "[requester] priority list query error");
}

[[nodiscard]] inline Rows FetchStatuses(quill::Logger *logger, sqlite3 *db) {
  return FetchLookupRows(logger, db,
                         "SELECT id, display_name FROM "
                         "helpdesk_ticket_status ORDER BY id",
                         "[requester] status list query error");
}

[[nodiscard]] inline Rows FetchTickets(quill::Logger *logger, sqlite3 *db,
                                       const std::string &profile_id,
                                       const TicketFilters &filters,
                                       const int page) {
  sqlite3_stmt *stmt = nullptr;
  std::string sql = R"SQL(
SELECT t.id, t.body, ts.display_name, p.display_name,
       strftime('%Y-%m-%d %H:%M:%S', datetime(t.created_at, '-5 hours')),
       strftime('%Y-%m-%d %H:%M:%S', datetime(t.updated_at, '-5 hours'))
  FROM helpdesk_ticket t
  JOIN helpdesk_ticket_status ts ON ts.id = t.status_id
  JOIN helpdesk_priority p       ON p.id  = t.priority_id
 WHERE t.requester_id = ?
)SQL";
  std::vector<std::string> values;
  if (!filters.search.empty()) {
    sql += " AND t.body LIKE ?";
    values.push_back("%" + filters.search + "%");
  }
  if (!filters.status_id.empty()) {
    sql += " AND t.status_id = ?";
    values.push_back(filters.status_id);
  }
  if (!filters.priority_id.empty()) {
    sql += " AND t.priority_id = ?";
    values.push_back(filters.priority_id);
  }
  sql += " ORDER BY ";
  sql += TicketSortExpression(filters.sort);
  sql += filters.sort_dir == "asc" ? " ASC" : " DESC";
  sql += ", t.id DESC LIMIT ? OFFSET ?";

  if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
    LOGJ_DEBUG(logger, "[requester] ticket list query error",
               sqlite3_errmsg(db));
    return {};
  }
  int bind_idx = 1;
  sqlite3_bind_text(stmt, bind_idx++, profile_id.c_str(), -1, SQLITE_STATIC);
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
                       {"created_at", ColumnText(stmt, 4)},
                       {"updated_at", ColumnText(stmt, 5)}});
  }
  sqlite3_finalize(stmt);
  return tickets;
}

[[nodiscard]] inline int CountTickets(quill::Logger *logger, sqlite3 *db,
                                      const std::string &profile_id,
                                      const TicketFilters &filters) {
  sqlite3_stmt *stmt = nullptr;
  std::string sql = "SELECT COUNT(*) FROM helpdesk_ticket t "
                    "WHERE t.requester_id = ?";
  std::vector<std::string> values;
  if (!filters.search.empty()) {
    sql += " AND t.body LIKE ?";
    values.push_back("%" + filters.search + "%");
  }
  if (!filters.status_id.empty()) {
    sql += " AND t.status_id = ?";
    values.push_back(filters.status_id);
  }
  if (!filters.priority_id.empty()) {
    sql += " AND t.priority_id = ?";
    values.push_back(filters.priority_id);
  }

  if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
    LOGJ_DEBUG(logger, "[requester] ticket count query error",
               sqlite3_errmsg(db));
    return 0;
  }

  int bind_idx = 1;
  sqlite3_bind_text(stmt, bind_idx++, profile_id.c_str(), -1, SQLITE_STATIC);
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
                    const std::string &profile_id,
                    const TicketFilters &filters,
                    const int requested_page) {
  TicketFilters normalized_filters = filters;
  normalized_filters.sort = NormalizeTicketSort(filters.sort);
  normalized_filters.sort_dir = NormalizeSortDir(filters.sort_dir);

  const int total_count = CountTickets(logger, db, profile_id,
                                       normalized_filters);
  const int page_count = PageCount(total_count);
  const int page = ClampPage(requested_page, page_count);
  auto tickets = FetchTickets(logger, db, profile_id, normalized_filters,
                              page);
  auto statuses = FetchStatuses(logger, db);
  auto priorities = FetchPriorities(logger, db);

  drogon::HttpViewData data;
  data.insert("tickets", tickets);
  data.insert("search", normalized_filters.search);
  data.insert("status_id", normalized_filters.status_id);
  data.insert("priority_id", normalized_filters.priority_id);
  data.insert("sort", normalized_filters.sort);
  data.insert("dir", normalized_filters.sort_dir);
  data.insert("statuses", statuses);
  data.insert("priorities", priorities);
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
                   const std::string &ticket_id,
                   const std::string &profile_id) {
  sqlite3_stmt *stmt = nullptr;
  const char *sql = R"SQL(
SELECT t.id, t.body, ts.display_name, p.display_name,
       strftime('%Y-%m-%d %H:%M:%S', datetime(t.created_at, '-5 hours')),
       COALESCE(t.due_date, ''),
       COALESCE(au.name, '') AS assigned_to_name, ts.trait
  FROM helpdesk_ticket t
  JOIN helpdesk_ticket_status ts ON ts.id = t.status_id
  JOIN helpdesk_priority p       ON p.id  = t.priority_id
  LEFT JOIN helpdesk_profile ap  ON ap.id = t.assigned_to_id
  LEFT JOIN auth_user au         ON au.id = ap.user_id
 WHERE t.id = ?
   AND t.requester_id = ?
 LIMIT 1
)SQL";

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    LOGJ_DEBUG(logger, "[requester] ticket details query error",
               sqlite3_errmsg(db));
    return std::nullopt;
  }
  sqlite3_bind_text(stmt, 1, ticket_id.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 2, profile_id.c_str(), -1, SQLITE_STATIC);

  std::optional<Row> ticket;
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    ticket = Row{{"id", ColumnText(stmt, 0)},
                 {"body", ColumnText(stmt, 1)},
                 {"status_name", ColumnText(stmt, 2)},
                 {"priority_name", ColumnText(stmt, 3)},
                 {"created_at", ColumnText(stmt, 4)},
                 {"due_date", ColumnText(stmt, 5)},
                 {"assigned_to_name", ColumnText(stmt, 6)},
                 {"status_trait", ColumnText(stmt, 7)}};
  }
  sqlite3_finalize(stmt);
  return ticket;
}

[[nodiscard]] inline std::optional<std::string>
FetchTicketTraitForRequester(quill::Logger *logger, sqlite3 *db,
                              const std::string &ticket_id,
                              const std::string &profile_id) {
  sqlite3_stmt *stmt = nullptr;
  const char *sql = R"SQL(
SELECT ts.trait
  FROM helpdesk_ticket t
  JOIN helpdesk_ticket_status ts ON ts.id = t.status_id
 WHERE t.id = ?
   AND t.requester_id = ?
 LIMIT 1
)SQL";
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK)
    return std::nullopt;
  sqlite3_bind_text(stmt, 1, ticket_id.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 2, profile_id.c_str(), -1, SQLITE_STATIC);
  std::optional<std::string> trait;
  if (sqlite3_step(stmt) == SQLITE_ROW)
    trait = ColumnText(stmt, 0);
  sqlite3_finalize(stmt);
  return trait;
}

inline constexpr std::size_t MaxActivityAttachmentSize = 25UL * 1024 * 1024;
inline constexpr std::size_t MaxTicketAttachmentSize = 75UL * 1024 * 1024;

[[nodiscard]] inline std::string InferMimeType(const std::string &file_name) {
  const auto ext = [](std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return s;
  }(std::filesystem::path(file_name).extension().string());
  static const std::unordered_map<std::string, std::string> types = {
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
  const auto it = types.find(ext);
  return it == types.end() ? "application/octet-stream" : it->second;
}

[[nodiscard]] inline Rows FetchAttachments(quill::Logger *logger, sqlite3 *db,
                                           const std::string &ticket_id) {
  sqlite3_stmt *stmt = nullptr;
  const char *sql = R"SQL(
SELECT taa.id, taa.ticket_activity_id, taa.file_name
  FROM helpdesk_ticket_activity_attachment taa
  JOIN helpdesk_ticket_activity ta ON ta.id = taa.ticket_activity_id
 WHERE ta.ticket_id = ?
 ORDER BY taa.created_at ASC, taa.id ASC
)SQL";
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    LOGJ_DEBUG(logger, "[requester] attachments query error",
               sqlite3_errmsg(db));
    return {};
  }
  sqlite3_bind_text(stmt, 1, ticket_id.c_str(), -1, SQLITE_STATIC);
  Rows rows;
  while (sqlite3_step(stmt) == SQLITE_ROW)
    rows.push_back({{"id", ColumnText(stmt, 0)},
                    {"activity_id", ColumnText(stmt, 1)},
                    {"file_name", ColumnText(stmt, 2)}});
  sqlite3_finalize(stmt);
  return rows;
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

[[nodiscard]] inline bool
TicketActivityBelongsToRequester(sqlite3 *db, const std::string &ticket_id,
                                 const std::string &activity_id,
                                 const std::string &profile_id) {
  sqlite3_stmt *stmt = nullptr;
  const char *sql = R"SQL(
SELECT 1
  FROM helpdesk_ticket_activity ta
  JOIN helpdesk_ticket t ON t.id = ta.ticket_id
 WHERE ta.id = ?
   AND ta.ticket_id = ?
   AND t.requester_id = ?
 LIMIT 1
)SQL";
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK)
    return false;
  sqlite3_bind_text(stmt, 1, activity_id.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 2, ticket_id.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 3, profile_id.c_str(), -1, SQLITE_STATIC);
  const bool ok = sqlite3_step(stmt) == SQLITE_ROW;
  sqlite3_finalize(stmt);
  return ok;
}

[[nodiscard]] inline Rows FetchActivities(quill::Logger *logger, sqlite3 *db,
                                          const std::string &ticket_id) {
  sqlite3_stmt *stmt = nullptr;
  const char *sql = R"SQL(
SELECT ta.id, ta.body,
       strftime('%Y-%m-%d %H:%M:%S', datetime(ta.created_at, '-5 hours')),
       u.name
  FROM helpdesk_ticket_activity ta
  JOIN helpdesk_profile p ON p.id = ta.profile_id
  JOIN auth_user u        ON u.id = p.user_id
 WHERE ta.ticket_id = ?
 ORDER BY ta.created_at ASC, ta.id ASC
)SQL";

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    LOGJ_DEBUG(logger, "[requester] activities query error",
               sqlite3_errmsg(db));
    return {};
  }
  sqlite3_bind_text(stmt, 1, ticket_id.c_str(), -1, SQLITE_STATIC);

  Rows activities;
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    activities.push_back({{"id", ColumnText(stmt, 0)},
                          {"body", ColumnText(stmt, 1)},
                          {"created_at", ColumnText(stmt, 2)},
                          {"profile_name", ColumnText(stmt, 3)}});
  }
  sqlite3_finalize(stmt);
  return activities;
}

[[nodiscard]] inline std::optional<std::string>
FetchDefaultStatusId(quill::Logger *logger, sqlite3 *db) {
  sqlite3_stmt *stmt = nullptr;
  if (sqlite3_prepare_v2(db,
                         "SELECT default_status_id FROM helpdesk_setting WHERE "
                         "name = 'default'",
                         -1, &stmt, nullptr) != SQLITE_OK) {
    LOGJ_DEBUG(logger, "[requester] default status query error",
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
    LOGJ_DEBUG(logger, "[requester] default priority query error",
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
    LOGJ_DEBUG(logger, "[requester] ticket body maxlength query error",
               sqlite3_errmsg(db));
    return 570;
  }
  int maxlength = 570;
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
    LOGJ_DEBUG(logger, "[requester] ticket due delta query error",
               sqlite3_errmsg(db));
    return 172800;
  }
  int delta = 172800;
  if (sqlite3_step(stmt) == SQLITE_ROW)
    delta = sqlite3_column_int(stmt, 0);
  sqlite3_finalize(stmt);
  return delta;
}

[[nodiscard]] inline int FetchTicketActivityBodyMaxlength(quill::Logger *logger,
                                                          sqlite3 *db) {
  sqlite3_stmt *stmt = nullptr;
  if (sqlite3_prepare_v2(db,
                         "SELECT ticket_activity_body_maxlength FROM "
                         "helpdesk_setting WHERE name = 'default'",
                         -1, &stmt, nullptr) != SQLITE_OK) {
    LOGJ_DEBUG(logger, "[requester] activity body maxlength query error",
               sqlite3_errmsg(db));
    return 170;
  }
  int maxlength = 170;
  if (sqlite3_step(stmt) == SQLITE_ROW)
    maxlength = sqlite3_column_int(stmt, 0);
  sqlite3_finalize(stmt);
  return maxlength;
}

[[nodiscard]] inline std::optional<std::string>
FetchDefaultAssignedToId(quill::Logger *logger, sqlite3 *db) {
  sqlite3_stmt *stmt = nullptr;
  if (sqlite3_prepare_v2(db,
                         "SELECT default_assigned_to_id FROM helpdesk_setting "
                         "WHERE name = 'default'",
                         -1, &stmt, nullptr) != SQLITE_OK) {
    LOGJ_DEBUG(logger, "[requester] default assigned_to query error",
               sqlite3_errmsg(db));
    return std::nullopt;
  }
  std::optional<std::string> id;
  if (sqlite3_step(stmt) == SQLITE_ROW)
    id = ColumnText(stmt, 0);
  sqlite3_finalize(stmt);
  return id;
}

[[nodiscard]] inline std::optional<TicketCreateInput>
ParseTicketCreateInput(const drogon::HttpRequestPtr &req) {
  TicketCreateInput input{req->getParameter("priority_id"),
                          req->getParameter("body")};
  if (input.priority_id.empty() || input.body.empty())
    return std::nullopt;
  return input;
}

} // namespace

namespace ticketeer {

void Requester::HomeGet(const drogon::HttpRequestPtr &req,
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
  callback(drogon::HttpResponse::newHttpViewResponse("requester_home", data));
}

void Requester::TicketListGet(const drogon::HttpRequestPtr &req,
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

  TicketFilters filters{.search = req->getParameter("s"),
                        .status_id = req->getParameter("status_id"),
                        .priority_id = req->getParameter("priority_id"),
                        .sort = req->getParameter("sort"),
                        .sort_dir = req->getParameter("dir")};
  const auto page = ParsePage(req->getParameter("p"));
  auto data = BuildTicketListData(logger, db, profile->id, filters, page);
  sqlite3_close(db);

  callback(
      drogon::HttpResponse::newHttpViewResponse("requester_ticket_list", data));
}

void Requester::TicketCreateGet(const drogon::HttpRequestPtr &req,
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

  auto priorities = FetchPriorities(logger, db);
  const auto body_maxlength = FetchTicketBodyMaxlength(logger, db);
  const auto default_ticket_priority_id =
      FetchDefaultTicketPriorityId(logger, db).value_or("");
  sqlite3_close(db);

  drogon::HttpViewData data;
  data.insert("priorities", priorities);
  data.insert("body_maxlength", std::to_string(body_maxlength));
  data.insert("default_ticket_priority_id", default_ticket_priority_id);
  callback(drogon::HttpResponse::newHttpViewResponse("requester_ticket_create",
                                                     data));
}

void Requester::TicketCreatePost(const drogon::HttpRequestPtr &req,
                                 Callback &&callback) {
  auto *logger = quill::Frontend::get_logger("root");
  const auto token = req->getCookie("token");

  const auto input = ParseTicketCreateInput(req);
  if (!input)
    return BadRequest(callback,
                      "Missing or invalid required fields: priority_id, "
                      "body");

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

  const auto body_maxlength = FetchTicketBodyMaxlength(logger, db);
  if (static_cast<int>(input->body.size()) > body_maxlength) {
    sqlite3_close(db);
    return BadRequest(callback, "Ticket body exceeds maximum length");
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

  const auto ticket_due_delta = FetchTicketDueDelta(logger, db);

  sqlite3_stmt *stmt = nullptr;
  const char *sql = R"SQL(
INSERT INTO helpdesk_ticket
  (requester_id, priority_id, status_id, assigned_to_id, body, due_date)
VALUES (?, ?, ?, ?, ?, date('now', '-5 hours', '+' || ? || ' seconds'))
)SQL";

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    LOGJ_DEBUG(logger, "[requester] insert ticket error", sqlite3_errmsg(db));
    sqlite3_close(db);
    return BadRequest(callback, "Failed to create ticket");
  }

  sqlite3_bind_text(stmt, 1, profile->id.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 2, input->priority_id.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 3, default_status_id->c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 4, default_assigned_to_id->c_str(), -1,
                    SQLITE_STATIC);
  sqlite3_bind_text(stmt, 5, input->body.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_int(stmt, 6, ticket_due_delta);

  if (sqlite3_step(stmt) != SQLITE_DONE) {
    LOGJ_DEBUG(logger, "[requester] insert ticket step error",
               sqlite3_errmsg(db));
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return BadRequest(callback, "Failed to create ticket");
  }
  sqlite3_finalize(stmt);

  auto data = BuildTicketListData(logger, db, profile->id, TicketFilters{}, 1);
  sqlite3_close(db);

  callback(
      drogon::HttpResponse::newHttpViewResponse("requester_ticket_list", data));
}

void Requester::TicketDetailsGet(const drogon::HttpRequestPtr &req,
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
  auto attachments = FetchAttachments(logger, db, ticket_id);
  const auto activity_body_maxlength =
      FetchTicketActivityBodyMaxlength(logger, db);
  sqlite3_close(db);

  drogon::HttpViewData data;
  data.insert("ticket", *ticket);
  data.insert("activities", activities);
  data.insert("attachments", attachments);
  data.insert("activity_body_maxlength",
              std::to_string(activity_body_maxlength));
  callback(drogon::HttpResponse::newHttpViewResponse("requester_ticket_details",
                                                     data));
}

void Requester::TicketActivityListGet(const drogon::HttpRequestPtr &req,
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
  auto attachments = FetchAttachments(logger, db, ticket_id);
  const auto activity_body_maxlength =
      FetchTicketActivityBodyMaxlength(logger, db);
  sqlite3_close(db);

  drogon::HttpViewData data;
  data.insert("ticket", *ticket);
  data.insert("activities", activities);
  data.insert("attachments", attachments);
  data.insert("activity_body_maxlength",
              std::to_string(activity_body_maxlength));
  callback(drogon::HttpResponse::newHttpViewResponse("requester_ticket_details",
                                                     data));
}

void Requester::TicketActivityCreateMessagePost(
    const drogon::HttpRequestPtr &req, Callback &&callback,
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
    return BadRequest(callback, "Invalid or expired token",
                      drogon::k401Unauthorized);
  }

  const auto ticket_trait =
      FetchTicketTraitForRequester(logger, db, ticket_id, profile->id);
  if (!ticket_trait) {
    sqlite3_close(db);
    return BadRequest(callback, "Ticket not found", drogon::k404NotFound);
  }
  if (*ticket_trait == "closed") {
    sqlite3_close(db);
    return BadRequest(callback, "Ticket is closed", drogon::k403Forbidden);
  }

  const auto activity_body_maxlength =
      FetchTicketActivityBodyMaxlength(logger, db);
  if (static_cast<int>(message.size()) > activity_body_maxlength) {
    sqlite3_close(db);
    return BadRequest(callback, "Activity body exceeds maximum length");
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
  sqlite3_bind_text(stmt, 2, message.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 3, profile->id.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 4, ticket_id.c_str(), -1, SQLITE_STATIC);

  if (sqlite3_step(stmt) != SQLITE_DONE || sqlite3_changes(db) != 1) {
    LOGJ_DEBUG(logger, "[requester] insert activity step error",
               sqlite3_errmsg(db));
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return BadRequest(callback, "Failed to create activity");
  }
  sqlite3_finalize(stmt);

  const auto last_id = std::to_string(sqlite3_last_insert_rowid(db));
  sqlite3_close(db);

  Row activity{{"id", last_id},
               {"body", message},
               {"created_at", "ahora"},
               {"profile_name", profile->name}};

  drogon::HttpViewData data;
  data.insert("ticket_id", ticket_id);
  data.insert("activity", activity);
  callback(drogon::HttpResponse::newHttpViewResponse(
      "requester_ticket_activity_message", data));
}

void Requester::TicketActivityAttachmentCreatePost(
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

  if (!TicketActivityBelongsToRequester(db, ticket_id, activity_id,
                                        profile->id)) {
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

  auto exec = [&](const char *sql) {
    return sqlite3_exec(db, sql, nullptr, nullptr, nullptr) == SQLITE_OK;
  };

  std::string html;
  for (const auto &upload_file : files) {
    const std::string fname =
        std::filesystem::path(upload_file.getFileName()).filename().string();
    if (fname.empty())
      continue;
    const auto fsize = upload_file.fileLength();
    const std::string mime = InferMimeType(fname);

    if (!exec("BEGIN IMMEDIATE"))
      continue;

    sqlite3_stmt *insert = nullptr;
    if (sqlite3_prepare_v2(db,
                           "INSERT INTO helpdesk_ticket_activity_attachment"
                           " (ticket_activity_id, file_path, file_name,"
                           "  file_size, mime_type)"
                           " VALUES (?, '', ?, ?, ?)",
                           -1, &insert, nullptr) != SQLITE_OK) {
      exec("ROLLBACK");
      continue;
    }
    sqlite3_bind_text(insert, 1, activity_id.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(insert, 2, fname.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int64(insert, 3, static_cast<sqlite3_int64>(fsize));
    sqlite3_bind_text(insert, 4, mime.c_str(), -1, SQLITE_STATIC);
    if (sqlite3_step(insert) != SQLITE_DONE) {
      sqlite3_finalize(insert);
      exec("ROLLBACK");
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
      exec("ROLLBACK");
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
      exec("ROLLBACK");
      continue;
    }
    sqlite3_finalize(upd);

    if (!exec("COMMIT")) {
      std::error_code ec;
      std::filesystem::remove(abs_path, ec);
      continue;
    }

    html += "<a class=\"badge badge-outline gap-1\" href=\"/ticketeer/requester"
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

void Requester::TicketActivityAttachmentDownloadGet(
    const drogon::HttpRequestPtr &req, Callback &&callback,
    const std::string &ticket_id, const std::string &activity_id,
    const std::string &attachment_id) {
  auto *logger = quill::Frontend::get_logger("root");
  const auto token = req->getCookie("token");

  sqlite3 *db = ConnectDB();
  if (!db)
    return BadRequest(callback, "Database unavailable",
                      drogon::k503ServiceUnavailable);

  sqlite3_stmt *stmt = nullptr;
  const char *sql = R"SQL(
SELECT taa.file_path, taa.file_name, taa.mime_type
  FROM helpdesk_ticket_activity_attachment taa
  JOIN helpdesk_ticket_activity ta ON ta.id = taa.ticket_activity_id
  JOIN helpdesk_ticket t            ON t.id  = ta.ticket_id
  JOIN helpdesk_profile p           ON p.id  = t.requester_id
  JOIN auth_session s               ON s.user_id = p.user_id
 WHERE taa.id = ?
   AND ta.id  = ?
   AND t.id   = ?
   AND s.token = ?
   AND s.expires_at > datetime('now')
   AND p.role = 'requester'
 LIMIT 1
)SQL";

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    LOGJ_DEBUG(logger, "[requester] attachment download query error",
               sqlite3_errmsg(db));
    sqlite3_close(db);
    return BadRequest(callback, "Attachment not found", drogon::k404NotFound);
  }
  sqlite3_bind_text(stmt, 1, attachment_id.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 2, activity_id.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 3, ticket_id.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 4, token.c_str(), -1, SQLITE_STATIC);

  std::string file_path, file_name, mime_type;
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    file_path = ColumnText(stmt, 0);
    file_name = ColumnText(stmt, 1);
    mime_type = ColumnText(stmt, 2);
  }
  sqlite3_finalize(stmt);
  sqlite3_close(db);

  if (file_name.empty())
    return BadRequest(callback, "Attachment not found", drogon::k404NotFound);

  const auto abs_path = AttachmentAbsolutePath(file_path, file_name);
  if (!std::filesystem::is_regular_file(abs_path))
    return BadRequest(callback, "Attachment file not found",
                      drogon::k404NotFound);

  callback(drogon::HttpResponse::newFileResponse(
      abs_path.string(), file_name, drogon::CT_CUSTOM, mime_type, req));
}

} // namespace ticketeer
