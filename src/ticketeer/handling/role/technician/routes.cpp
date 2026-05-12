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
#include <sqlite3.h>
#include <trantor/utils/Logger.h>

#include "ticketeer/core/conf.hpp"
#include "ticketeer/core/db.hpp"
#include "ticketeer/core/text.hpp"
#include "ticketeer/handling/common.hpp"
#include "ticketeer/handling/utils.hpp"

namespace {

using ticketeer::core::db::ColumnText;
using ticketeer::core::db::ConnectDB;
using ticketeer::core::db::Row;
using ticketeer::core::db::Rows;
using ticketeer::handling::common::BadRequest;
using ticketeer::handling::common::Callback;
using ticketeer::handling::common::FetchActivityAttachmentTotal;
using ticketeer::handling::common::FetchDefaultAssignedToId;
using ticketeer::handling::common::FetchDefaultStatusId;
using ticketeer::handling::common::FetchDefaultTicketPriorityId;
using ticketeer::handling::common::FetchLookupRows;
using ticketeer::handling::common::FetchPriorities;
using ticketeer::handling::common::FetchStatuses;
using ticketeer::handling::common::FetchTicketActivityBodyMaxlength;
using ticketeer::handling::common::FetchTicketAttachmentTotal;
using ticketeer::handling::common::FetchTicketBodyMaxlength;
using ticketeer::handling::common::FetchTicketDueDelta;
using ticketeer::handling::utils::AttachmentAbsolutePath;
using ticketeer::handling::utils::ClampPage;
using ticketeer::handling::utils::InferMimeType;
using ticketeer::handling::utils::MaxActivityAttachmentSize;
using ticketeer::handling::utils::MaxTicketAttachmentSize;
using ticketeer::handling::utils::NormalizeSortDir;
using ticketeer::handling::utils::PageCount;
using ticketeer::handling::utils::ParsePage;
using ticketeer::handling::utils::TicketPageSize;
using ticketeer::handling::utils::WriteFile;

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

[[nodiscard]] inline std::string NormalizeTicketSort(std::string_view sort) {
  if (sort == "body" || sort == "status" || sort == "priority" ||
      sort == "created_at" || sort == "updated_at")
    return std::string(sort);
  return "created_at";
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

[[nodiscard]] inline std::optional<Profile>
FetchProfile(sqlite3 *db, const std::string &token) {
  sqlite3_stmt *stmt = nullptr;
  const char *sql = R"SQL(
SELECT p.id, u.username, u.name
  FROM helpdesk_profile p
  JOIN auth_session s ON s.user_id = p.user_id
  JOIN auth_user u    ON u.id = p.user_id
 WHERE s.token = ?
   AND s.expires_at > datetime('now')
   AND p.role = 'technician'
 LIMIT 1
)SQL";

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    LOG_DEBUG << "[technician] profile query error: " << sqlite3_errmsg(db);
    return std::nullopt;
  }
  sqlite3_bind_text(stmt, 1, token.c_str(), -1, SQLITE_STATIC);

  std::optional<Profile> profile;
  if (sqlite3_step(stmt) == SQLITE_ROW)
    profile =
        Profile{ColumnText(stmt, 0), ColumnText(stmt, 1), ColumnText(stmt, 2)};
  else
    LOG_DEBUG << "[technician] profile not found";

  sqlite3_finalize(stmt);
  return profile;
}

[[nodiscard]] inline Rows FetchTickets(sqlite3 *db,
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
 WHERE t.assigned_to_id = ?
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
    LOG_DEBUG << "[technician] ticket list query error: " << sqlite3_errmsg(db);
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

[[nodiscard]] inline int CountTickets(sqlite3 *db,
                                      const std::string &profile_id,
                                      const TicketFilters &filters) {
  sqlite3_stmt *stmt = nullptr;
  std::string sql = "SELECT COUNT(*) FROM helpdesk_ticket t "
                    "WHERE t.assigned_to_id = ?";
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
    LOG_DEBUG << "[technician] ticket count query error: "
              << sqlite3_errmsg(db);
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
BuildTicketListData(sqlite3 *db, const std::string &profile_id,
                    const TicketFilters &filters, const int requested_page) {
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

[[nodiscard]] inline std::optional<Row>
FetchTicketDetails(sqlite3 *db, const std::string &ticket_id,
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
   AND t.assigned_to_id = ?
 LIMIT 1
)SQL";

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    LOG_DEBUG << "[technician] ticket details query error: "
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

[[nodiscard]] inline std::optional<std::string>
FetchTicketTraitForTechnician(sqlite3 *db, const std::string &ticket_id,
                              const std::string &profile_id) {
  sqlite3_stmt *stmt = nullptr;
  const char *sql = R"SQL(
SELECT ts.trait
  FROM helpdesk_ticket t
  JOIN helpdesk_ticket_status ts ON ts.id = t.status_id
 WHERE t.id = ?
   AND t.assigned_to_id = ?
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

[[nodiscard]] inline Rows FetchAttachments(sqlite3 *db,
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
    LOG_DEBUG << "[technician] attachments query error: " << sqlite3_errmsg(db);
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

[[nodiscard]] inline bool
TicketActivityBelongsToTechnician(sqlite3 *db, const std::string &ticket_id,
                                  const std::string &activity_id,
                                  const std::string &profile_id) {
  sqlite3_stmt *stmt = nullptr;
  const char *sql = R"SQL(
SELECT 1
  FROM helpdesk_ticket_activity ta
  JOIN helpdesk_ticket t ON t.id = ta.ticket_id
 WHERE ta.id = ?
   AND ta.ticket_id = ?
   AND t.assigned_to_id = ?
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

[[nodiscard]] inline Rows FetchActivities(sqlite3 *db,
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
    LOG_DEBUG << "[technician] activities query error: " << sqlite3_errmsg(db);
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

void Technician::HomeGet(const drogon::HttpRequestPtr &req,
                         Callback &&callback) {
  const auto token = req->getCookie("token");

  sqlite3 *db = ConnectDB();
  if (!db)
    return BadRequest(callback, "Database unavailable",
                      drogon::k503ServiceUnavailable);

  const auto profile = FetchProfile(db, token);
  sqlite3_close(db);

  if (!profile)
    return callback(
        drogon::HttpResponse::newRedirectionResponse("/ticketeer/auth/signin"));

  drogon::HttpViewData data;
  data.insert("username", profile->username);
  data.insert("name", profile->name);
  callback(drogon::HttpResponse::newHttpViewResponse("technician_home", data));
}

void Technician::TicketListGet(const drogon::HttpRequestPtr &req,
                               Callback &&callback) {
  const auto token = req->getCookie("token");

  sqlite3 *db = ConnectDB();
  if (!db)
    return BadRequest(callback, "Database unavailable",
                      drogon::k503ServiceUnavailable);

  const auto profile = FetchProfile(db, token);
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
  auto data = BuildTicketListData(db, profile->id, filters, page);
  sqlite3_close(db);

  callback(drogon::HttpResponse::newHttpViewResponse("technician_ticket_list",
                                                     data));
}

void Technician::TicketCreateGet(const drogon::HttpRequestPtr &req,
                                 Callback &&callback) {
  const auto token = req->getCookie("token");

  sqlite3 *db = ConnectDB();
  if (!db)
    return BadRequest(callback, "Database unavailable",
                      drogon::k503ServiceUnavailable);

  const auto profile = FetchProfile(db, token);
  if (!profile) {
    sqlite3_close(db);
    return callback(
        drogon::HttpResponse::newRedirectionResponse("/ticketeer/auth/signin"));
  }

  auto priorities = FetchPriorities(db);
  const auto body_maxlength = FetchTicketBodyMaxlength(db);
  const auto default_ticket_priority_id =
      FetchDefaultTicketPriorityId(db).value_or("");
  sqlite3_close(db);

  drogon::HttpViewData data;
  data.insert("priorities", priorities);
  data.insert("body_maxlength", std::to_string(body_maxlength));
  data.insert("default_ticket_priority_id", default_ticket_priority_id);
  callback(drogon::HttpResponse::newHttpViewResponse("technician_ticket_create",
                                                     data));
}

void Technician::TicketCreatePost(const drogon::HttpRequestPtr &req,
                                  Callback &&callback) {
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

  const auto profile = FetchProfile(db, token);
  if (!profile) {
    sqlite3_close(db);
    return BadRequest(callback, "Invalid or expired token",
                      drogon::k401Unauthorized);
  }

  const auto body_maxlength = FetchTicketBodyMaxlength(db);
  if (static_cast<int>(input->body.size()) > body_maxlength) {
    sqlite3_close(db);
    return BadRequest(callback, "Ticket body exceeds maximum length");
  }

  const auto default_status_id = FetchDefaultStatusId(db);
  if (!default_status_id) {
    sqlite3_close(db);
    return BadRequest(callback, "Failed to get default status");
  }

  const auto default_assigned_to_id = FetchDefaultAssignedToId(db);
  if (!default_assigned_to_id) {
    sqlite3_close(db);
    return BadRequest(callback, "Failed to get default assigned to");
  }

  const auto ticket_due_delta = FetchTicketDueDelta(db);

  sqlite3_stmt *stmt = nullptr;
  const char *sql = R"SQL(
INSERT INTO helpdesk_ticket
  (requester_id, priority_id, status_id, assigned_to_id, body, due_date)
VALUES (?, ?, ?, ?, ?, date('now', '-5 hours', '+' || ? || ' seconds'))
)SQL";

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    LOG_DEBUG << "[technician] insert ticket error: " << sqlite3_errmsg(db);
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
    LOG_DEBUG << "[technician] insert ticket step error: "
              << sqlite3_errmsg(db);
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return BadRequest(callback, "Failed to create ticket");
  }
  sqlite3_finalize(stmt);

  auto data = BuildTicketListData(db, profile->id, TicketFilters{}, 1);
  sqlite3_close(db);

  callback(drogon::HttpResponse::newHttpViewResponse("technician_ticket_list",
                                                     data));
}

void Technician::TicketDetailsGet(const drogon::HttpRequestPtr &req,
                                  Callback &&callback,
                                  const std::string &ticket_id) {
  const auto token = req->getCookie("token");

  sqlite3 *db = ConnectDB();
  if (!db)
    return BadRequest(callback, "Database unavailable",
                      drogon::k503ServiceUnavailable);

  const auto profile = FetchProfile(db, token);
  if (!profile) {
    sqlite3_close(db);
    return BadRequest(callback, "Forbidden", drogon::k403Forbidden);
  }

  const auto ticket = FetchTicketDetails(db, ticket_id, profile->id);
  if (!ticket) {
    sqlite3_close(db);
    return BadRequest(callback, "Ticket not found", drogon::k404NotFound);
  }

  auto activities = FetchActivities(db, ticket_id);
  auto attachments = FetchAttachments(db, ticket_id);
  const auto activity_body_maxlength = FetchTicketActivityBodyMaxlength(db);
  sqlite3_close(db);

  drogon::HttpViewData data;
  data.insert("ticket", *ticket);
  data.insert("activities", activities);
  data.insert("attachments", attachments);
  data.insert("activity_body_maxlength",
              std::to_string(activity_body_maxlength));
  callback(drogon::HttpResponse::newHttpViewResponse(
      "technician_ticket_details", data));
}

void Technician::TicketActivityListGet(const drogon::HttpRequestPtr &req,
                                       Callback &&callback,
                                       const std::string &ticket_id) {
  const auto token = req->getCookie("token");

  sqlite3 *db = ConnectDB();
  if (!db)
    return BadRequest(callback, "Database unavailable",
                      drogon::k503ServiceUnavailable);

  const auto profile = FetchProfile(db, token);
  if (!profile) {
    sqlite3_close(db);
    return BadRequest(callback, "Forbidden", drogon::k403Forbidden);
  }

  const auto ticket = FetchTicketDetails(db, ticket_id, profile->id);
  if (!ticket) {
    sqlite3_close(db);
    return BadRequest(callback, "Ticket not found", drogon::k404NotFound);
  }

  auto activities = FetchActivities(db, ticket_id);
  auto attachments = FetchAttachments(db, ticket_id);
  const auto activity_body_maxlength = FetchTicketActivityBodyMaxlength(db);
  sqlite3_close(db);

  drogon::HttpViewData data;
  data.insert("ticket", *ticket);
  data.insert("activities", activities);
  data.insert("attachments", attachments);
  data.insert("activity_body_maxlength",
              std::to_string(activity_body_maxlength));
  callback(drogon::HttpResponse::newHttpViewResponse(
      "technician_ticket_details", data));
}

void Technician::TicketActivityCreateMessagePost(
    const drogon::HttpRequestPtr &req, Callback &&callback,
    const std::string &ticket_id) {
  const auto token = req->getCookie("token");

  const auto message = req->getParameter("message");
  if (message.empty())
    return BadRequest(callback, "Missing field: message");

  sqlite3 *db = ConnectDB();
  if (!db)
    return BadRequest(callback, "Database unavailable",
                      drogon::k503ServiceUnavailable);

  const auto profile = FetchProfile(db, token);
  if (!profile) {
    sqlite3_close(db);
    return BadRequest(callback, "Invalid or expired token",
                      drogon::k401Unauthorized);
  }

  const auto ticket_trait =
      FetchTicketTraitForTechnician(db, ticket_id, profile->id);
  if (!ticket_trait) {
    sqlite3_close(db);
    return BadRequest(callback, "Ticket not found", drogon::k404NotFound);
  }
  if (*ticket_trait == "closed") {
    sqlite3_close(db);
    return BadRequest(callback, "Ticket is closed", drogon::k403Forbidden);
  }

  const auto activity_body_maxlength = FetchTicketActivityBodyMaxlength(db);
  if (static_cast<int>(ticketeer::core::text::Utf8Length(message)) >
      activity_body_maxlength) {
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
   AND t.assigned_to_id = p.id
)SQL";

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    LOG_DEBUG << "[technician] insert activity error: " << sqlite3_errmsg(db);
    sqlite3_close(db);
    return BadRequest(callback, "Failed to create activity");
  }
  sqlite3_bind_text(stmt, 1, ticket_id.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 2, message.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 3, profile->id.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 4, ticket_id.c_str(), -1, SQLITE_STATIC);

  if (sqlite3_step(stmt) != SQLITE_DONE || sqlite3_changes(db) != 1) {
    LOG_DEBUG << "[technician] insert activity step error: "
              << sqlite3_errmsg(db);
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
      "technician_ticket_activity_message", data));
}

void Technician::TicketActivityAttachmentCreatePost(
    const drogon::HttpRequestPtr &req, Callback &&callback,
    const std::string &ticket_id, const std::string &activity_id) {
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

  const auto profile = FetchProfile(db, token);
  if (!profile) {
    sqlite3_close(db);
    return BadRequest(callback, "Forbidden", drogon::k403Forbidden);
  }

  if (!TicketActivityBelongsToTechnician(db, ticket_id, activity_id,
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

    html +=
        "<a class=\"badge badge-outline gap-1\" href=\"/ticketeer/technician"
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

void Technician::TicketActivityAttachmentDownloadGet(
    const drogon::HttpRequestPtr &req, Callback &&callback,
    const std::string &ticket_id, const std::string &activity_id,
    const std::string &attachment_id) {
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
  JOIN helpdesk_profile p           ON p.id  = t.assigned_to_id
  JOIN auth_session s               ON s.user_id = p.user_id
 WHERE taa.id = ?
   AND ta.id  = ?
   AND t.id   = ?
   AND s.token = ?
   AND s.expires_at > datetime('now')
   AND p.role = 'technician'
 LIMIT 1
)SQL";

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    LOG_DEBUG << "[technician] attachment download query error: "
              << sqlite3_errmsg(db);
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
