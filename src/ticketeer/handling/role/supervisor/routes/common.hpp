#pragma once

#include <cstddef>
#include <filesystem>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>
#include <drogon/HttpViewData.h>
#include <sqlite3.h>

#include "ticketeer/core/db.hpp"
#include "ticketeer/handling/common.hpp"
#include "ticketeer/handling/utils.hpp"

namespace ticketeer::handling::role::supervisor::routes::common {

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

[[nodiscard]] bool IsValidIsoDate(std::string_view value);

[[nodiscard]] std::optional<Profile> FetchProfile(sqlite3 *db,
                                                  const std::string &token);

[[nodiscard]] Rows FetchAssignees(sqlite3 *db);

[[nodiscard]] drogon::HttpViewData
BuildTicketListData(sqlite3 *db, const TicketFilters &filters,
                    const int requested_page);

[[nodiscard]] std::optional<Row>
FetchTicketDetails(sqlite3 *db, const std::string &ticket_id);

[[nodiscard]] std::optional<std::string>
FetchTicketTrait(sqlite3 *db, const std::string &ticket_id);

[[nodiscard]] std::optional<std::string>
FetchStatusTrait(sqlite3 *db, const std::string &id);

[[nodiscard]] std::optional<std::string> FetchStatusName(sqlite3 *db,
                                                         const std::string &id);

[[nodiscard]] bool UpdateTicketStatus(sqlite3 *db, const std::string &ticket_id,
                                      const std::string &new_status);

[[nodiscard]] std::optional<std::string>
FetchPriorityName(sqlite3 *db, const std::string &id);

void InsertTicketActivity(sqlite3 *db, const std::string &ticket_id,
                          const std::string &profile_id,
                          const std::string &kind, const std::string &body);

[[nodiscard]] bool UpdateTicketPriority(sqlite3 *db,
                                        const std::string &ticket_id,
                                        const std::string &new_priority);

[[nodiscard]] bool UpdateTicketAssignedTo(sqlite3 *db,
                                          const std::string &ticket_id,
                                          const std::string &new_assigned_to);

[[nodiscard]] bool UpdateTicketDueDate(sqlite3 *db,
                                       const std::string &ticket_id,
                                       const std::string &new_due_date);

[[nodiscard]] Rows FetchActivities(sqlite3 *db, const std::string &ticket_id);

[[nodiscard]] Rows FetchAttachments(sqlite3 *db, const std::string &ticket_id);

[[nodiscard]] bool TicketExists(sqlite3 *db, const std::string &ticket_id);

[[nodiscard]] bool TicketActivityExists(sqlite3 *db,
                                        const std::string &ticket_id,
                                        const std::string &activity_id);

[[nodiscard]] std::optional<TicketCreateInput>
ParseTicketCreateInput(const drogon::HttpRequestPtr &req);

[[nodiscard]] std::optional<AttachmentFile>
FetchTicketActivityAttachmentFile(sqlite3 *db, const std::string &ticket_id,
                                  const std::string &activity_id,
                                  const std::string &attachment_id);

bool Exec(sqlite3 *db, const char *sql);

[[nodiscard]] std::string Trim(std::string value);

[[nodiscard]] std::optional<TicketStatusInput>
ParseTicketStatusInput(const drogon::HttpRequestPtr &req);

[[nodiscard]] std::optional<TicketPriorityInput>
ParseTicketPriorityInput(const drogon::HttpRequestPtr &req);

[[nodiscard]] bool TicketStatusExists(sqlite3 *db, const std::string &id);

[[nodiscard]] bool ReassignTicketStatus(sqlite3 *db, const std::string &from_id,
                                        const std::string &to_id);

[[nodiscard]] bool InsertTicketStatus(sqlite3 *db,
                                      const TicketStatusInput &input);

[[nodiscard]] bool UpdateTicketStatusRecord(sqlite3 *db, const std::string &id,
                                            const TicketStatusInput &input);

[[nodiscard]] bool DeleteTicketStatus(sqlite3 *db, const std::string &id);

[[nodiscard]] bool TicketPriorityExists(sqlite3 *db, const std::string &id);

[[nodiscard]] bool ReassignTicketPriority(sqlite3 *db,
                                          const std::string &from_id,
                                          const std::string &to_id);

[[nodiscard]] bool InsertTicketPriority(sqlite3 *db,
                                        const TicketPriorityInput &input);

[[nodiscard]] bool UpdateTicketPriorityRecord(sqlite3 *db,
                                              const std::string &id,
                                              const TicketPriorityInput &input);

[[nodiscard]] bool DeleteTicketPriority(sqlite3 *db, const std::string &id);

[[nodiscard]] std::optional<int> ParsePositiveInt(const std::string &value);

[[nodiscard]] bool
UpsertDefaultSetting(sqlite3 *db, const std::string &default_status_id,
                     const std::string &default_assigned_to_id,
                     const std::string &default_ticket_priority_id,
                     const std::string &assigned_status_id,
                     const std::string &system_profile_id,
                     int ticket_body_maxlength,
                     int ticket_activity_body_maxlength, int ticket_due_delta);

void RenderSettingsConfig(const Callback &callback, sqlite3 *db);

[[nodiscard]] bool RenderTicketStatusConfig(const Callback &callback,
                                            sqlite3 *db,
                                            const std::string &editing_id);

[[nodiscard]] bool RenderTicketPriorityConfig(const Callback &callback,
                                              sqlite3 *db,
                                              const std::string &editing_id);

} // namespace ticketeer::handling::role::supervisor::routes::common
