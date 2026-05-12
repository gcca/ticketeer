#include "common.hpp"

#include <trantor/utils/Logger.h>

namespace ticketeer::handling::common {

using ticketeer::core::db::ColumnText;

void BadRequest(const Callback &callback, const char *msg,
                drogon::HttpStatusCode code) {
  auto resp = drogon::HttpResponse::newHttpResponse();
  resp->setStatusCode(code);
  resp->setBody(msg);
  callback(resp);
}

Rows FetchLookupRows(sqlite3 *db, const char *sql, const char *log_message) {
  sqlite3_stmt *stmt = nullptr;
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    LOG_DEBUG << "[handling] lookup query error: " << log_message << ": "
              << sqlite3_errmsg(db);
    return {};
  }

  Rows rows;
  while (sqlite3_step(stmt) == SQLITE_ROW)
    rows.push_back(
        {{"id", ColumnText(stmt, 0)}, {"name", ColumnText(stmt, 1)}});

  sqlite3_finalize(stmt);
  return rows;
}

Rows FetchPriorities(sqlite3 *db) {
  return FetchLookupRows(db,
                         "SELECT id, display_name FROM helpdesk_priority ORDER "
                         "BY id",
                         "[handling] priority list query error");
}

Rows FetchStatuses(sqlite3 *db) {
  return FetchLookupRows(db,
                         "SELECT id, display_name FROM "
                         "helpdesk_ticket_status ORDER BY id",
                         "[handling] status list query error");
}

std::optional<std::string> FetchDefaultStatusId(sqlite3 *db) {
  sqlite3_stmt *stmt = nullptr;
  if (sqlite3_prepare_v2(db,
                         "SELECT default_status_id FROM helpdesk_setting WHERE "
                         "name = 'default'",
                         -1, &stmt, nullptr) != SQLITE_OK) {
    LOG_DEBUG << "[handling] default status query error: "
              << sqlite3_errmsg(db);
    return std::nullopt;
  }

  std::optional<std::string> id;
  if (sqlite3_step(stmt) == SQLITE_ROW)
    id = ColumnText(stmt, 0);

  sqlite3_finalize(stmt);
  return id;
}

std::optional<std::string> FetchDefaultTicketPriorityId(sqlite3 *db) {
  sqlite3_stmt *stmt = nullptr;
  if (sqlite3_prepare_v2(db,
                         "SELECT default_ticket_priority_id FROM "
                         "helpdesk_setting WHERE name = 'default'",
                         -1, &stmt, nullptr) != SQLITE_OK) {
    LOG_DEBUG << "[handling] default priority query error: "
              << sqlite3_errmsg(db);
    return std::nullopt;
  }

  std::optional<std::string> id;
  if (sqlite3_step(stmt) == SQLITE_ROW)
    id = ColumnText(stmt, 0);

  sqlite3_finalize(stmt);
  return id;
}

std::optional<std::string> FetchDefaultAssignedToId(sqlite3 *db) {
  sqlite3_stmt *stmt = nullptr;
  if (sqlite3_prepare_v2(db,
                         "SELECT default_assigned_to_id FROM helpdesk_setting "
                         "WHERE name = 'default'",
                         -1, &stmt, nullptr) != SQLITE_OK) {
    LOG_DEBUG << "[handling] default assigned_to query error: "
              << sqlite3_errmsg(db);
    return std::nullopt;
  }

  std::optional<std::string> id;
  if (sqlite3_step(stmt) == SQLITE_ROW)
    id = ColumnText(stmt, 0);

  sqlite3_finalize(stmt);
  return id;
}

int FetchTicketBodyMaxlength(sqlite3 *db) {
  sqlite3_stmt *stmt = nullptr;
  if (sqlite3_prepare_v2(db,
                         "SELECT ticket_body_maxlength FROM helpdesk_setting "
                         "WHERE name = 'default'",
                         -1, &stmt, nullptr) != SQLITE_OK) {
    LOG_DEBUG << "[handling] ticket body maxlength query error: "
              << sqlite3_errmsg(db);
    return 570;
  }
  int maxlength = 570;
  if (sqlite3_step(stmt) == SQLITE_ROW)
    maxlength = sqlite3_column_int(stmt, 0);
  sqlite3_finalize(stmt);
  return maxlength;
}

int FetchTicketActivityBodyMaxlength(sqlite3 *db) {
  sqlite3_stmt *stmt = nullptr;
  if (sqlite3_prepare_v2(db,
                         "SELECT ticket_activity_body_maxlength FROM "
                         "helpdesk_setting WHERE name = 'default'",
                         -1, &stmt, nullptr) != SQLITE_OK) {
    LOG_DEBUG << "[handling] activity body maxlength query error: "
              << sqlite3_errmsg(db);
    return 170;
  }
  int maxlength = 170;
  if (sqlite3_step(stmt) == SQLITE_ROW)
    maxlength = sqlite3_column_int(stmt, 0);
  sqlite3_finalize(stmt);
  return maxlength;
}

int FetchTicketDueDelta(sqlite3 *db) {
  sqlite3_stmt *stmt = nullptr;
  if (sqlite3_prepare_v2(db,
                         "SELECT ticket_due_delta FROM helpdesk_setting "
                         "WHERE name = 'default'",
                         -1, &stmt, nullptr) != SQLITE_OK) {
    LOG_DEBUG << "[handling] ticket due delta query error: "
              << sqlite3_errmsg(db);
    return 172800;
  }
  int delta = 172800;
  if (sqlite3_step(stmt) == SQLITE_ROW)
    delta = sqlite3_column_int(stmt, 0);
  sqlite3_finalize(stmt);
  return delta;
}

std::size_t FetchActivityAttachmentTotal(sqlite3 *db,
                                         const std::string &activity_id) {
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

std::size_t FetchTicketAttachmentTotal(sqlite3 *db,
                                       const std::string &ticket_id) {
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

} // namespace ticketeer::handling::common
