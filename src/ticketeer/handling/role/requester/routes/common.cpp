#include "common.hpp"

#include <string_view>
#include <vector>

#include <trantor/utils/Logger.h>

namespace ticketeer::handling::role::requester::routes::common {
namespace {

[[nodiscard]] std::string NormalizeTicketSort(std::string_view sort) {
  if (sort == "body" || sort == "status" || sort == "priority" ||
      sort == "created_at" || sort == "updated_at")
    return std::string(sort);
  return "created_at";
}

[[nodiscard]] const char *TicketSortExpression(const std::string &sort) {
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

[[nodiscard]] Rows FetchTickets(sqlite3 *db, const std::string &profile_id,
                                const TicketFilters &filters, const int page) {
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
    LOG_DEBUG << "[requester] ticket list query error: " << sqlite3_errmsg(db);
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

[[nodiscard]] int CountTickets(sqlite3 *db, const std::string &profile_id,
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
    LOG_DEBUG << "[requester] ticket count query error: " << sqlite3_errmsg(db);
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

} // namespace

std::optional<Profile> FetchProfile(sqlite3 *db, const std::string &token) {
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
    LOG_DEBUG << "[requester] profile query error: " << sqlite3_errmsg(db);
    return std::nullopt;
  }
  sqlite3_bind_text(stmt, 1, token.c_str(), -1, SQLITE_STATIC);

  std::optional<Profile> profile;
  if (sqlite3_step(stmt) == SQLITE_ROW)
    profile =
        Profile{ColumnText(stmt, 0), ColumnText(stmt, 1), ColumnText(stmt, 2)};
  else
    LOG_DEBUG << "[requester] profile not found";

  sqlite3_finalize(stmt);
  return profile;
}

drogon::HttpViewData BuildTicketListData(sqlite3 *db,
                                         const std::string &profile_id,
                                         const TicketFilters &filters,
                                         const int requested_page) {
  TicketFilters normalized_filters = filters;
  normalized_filters.sort = NormalizeTicketSort(filters.sort);
  normalized_filters.sort_dir = NormalizeSortDir(filters.sort_dir);

  const int total_count = CountTickets(db, profile_id, normalized_filters);
  const int page_count = PageCount(total_count);
  const int page = ClampPage(requested_page, page_count);
  auto tickets = FetchTickets(db, profile_id, normalized_filters, page);
  auto statuses = FetchStatuses(db);
  auto priorities = FetchPriorities(db);

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

std::optional<Row> FetchTicketDetails(sqlite3 *db, const std::string &ticket_id,
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
    LOG_DEBUG << "[requester] ticket details query error: "
              << sqlite3_errmsg(db);
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

std::optional<std::string>
FetchTicketTraitForRequester(sqlite3 *db, const std::string &ticket_id,
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

Rows FetchAttachments(sqlite3 *db, const std::string &ticket_id) {
  sqlite3_stmt *stmt = nullptr;
  const char *sql = R"SQL(
SELECT taa.id, taa.ticket_activity_id, taa.file_name
  FROM helpdesk_ticket_activity_attachment taa
  JOIN helpdesk_ticket_activity ta ON ta.id = taa.ticket_activity_id
 WHERE ta.ticket_id = ?
 ORDER BY taa.created_at ASC, taa.id ASC
)SQL";
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    LOG_DEBUG << "[requester] attachments query error: " << sqlite3_errmsg(db);
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

bool TicketActivityBelongsToRequester(sqlite3 *db, const std::string &ticket_id,
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

Rows FetchActivities(sqlite3 *db, const std::string &ticket_id) {
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
    LOG_DEBUG << "[requester] activities query error: " << sqlite3_errmsg(db);
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

std::optional<TicketCreateInput>
ParseTicketCreateInput(const drogon::HttpRequestPtr &req) {
  TicketCreateInput input{req->getParameter("priority_id"),
                          req->getParameter("body")};
  if (input.priority_id.empty() || input.body.empty())
    return std::nullopt;
  return input;
}

} // namespace ticketeer::handling::role::requester::routes::common
