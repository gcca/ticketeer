#pragma once

#include <optional>
#include <string>

#include <drogon/HttpRequest.h>
#include <drogon/HttpViewData.h>
#include <sqlite3.h>

#include "ticketeer/core/db.hpp"
#include "ticketeer/handling/common.hpp"
#include "ticketeer/handling/utils.hpp"

namespace ticketeer::handling::role::technician::routes::common {

using ticketeer::core::db::ColumnText;
using ticketeer::core::db::Connect;
using ticketeer::core::db::ConnectRO;
using ticketeer::core::db::Row;
using ticketeer::core::db::Rows;
using ticketeer::handling::common::BadRequest;
using ticketeer::handling::common::Callback;
using ticketeer::handling::common::FetchActivityAttachmentTotal;
using ticketeer::handling::common::FetchDefaultAssignedToId;
using ticketeer::handling::common::FetchDefaultStatusId;
using ticketeer::handling::common::FetchDefaultTicketPriorityId;
using ticketeer::handling::common::FetchPriorities;
using ticketeer::handling::common::FetchStatuses;
using ticketeer::handling::common::FetchTicketActivityAttachmentMaxsize;
using ticketeer::handling::common::FetchTicketActivityBodyMaxlength;
using ticketeer::handling::common::FetchTicketAttachmentMaxsize;
using ticketeer::handling::common::FetchTicketAttachmentTotal;
using ticketeer::handling::common::FetchTicketBodyMaxlength;
using ticketeer::handling::common::FetchTicketDueDelta;
using ticketeer::handling::utils::AttachmentAbsolutePath;
using ticketeer::handling::utils::ClampPage;
using ticketeer::handling::utils::InferMimeType;
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

[[nodiscard]] std::optional<Profile> FetchProfile(sqlite3 *db,
                                                  const std::string &token);

[[nodiscard]] drogon::HttpViewData
BuildTicketListData(sqlite3 *db, const std::string &profile_id,
                    const TicketFilters &filters, int requested_page);

[[nodiscard]] std::optional<Row>
FetchTicketDetails(sqlite3 *db, const std::string &ticket_id,
                   const std::string &profile_id);

[[nodiscard]] std::optional<std::string>
FetchTicketTraitForTechnician(sqlite3 *db, const std::string &ticket_id,
                              const std::string &profile_id);

[[nodiscard]] Rows FetchAttachments(sqlite3 *db, const std::string &ticket_id);

[[nodiscard]] bool
TicketActivityBelongsToTechnician(sqlite3 *db, const std::string &ticket_id,
                                  const std::string &activity_id,
                                  const std::string &profile_id);

[[nodiscard]] Rows FetchActivities(sqlite3 *db, const std::string &ticket_id);

[[nodiscard]] std::optional<TicketCreateInput>
ParseTicketCreateInput(const drogon::HttpRequestPtr &req);

} // namespace ticketeer::handling::role::technician::routes::common
