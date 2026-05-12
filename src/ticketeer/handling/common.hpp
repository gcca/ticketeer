#pragma once

#include <cstddef>
#include <functional>
#include <optional>
#include <string>

#include <drogon/HttpResponse.h>
#include <sqlite3.h>

#include "ticketeer/core/db.hpp"

namespace ticketeer::handling::common {

using Callback = std::function<void(const drogon::HttpResponsePtr &)>;

using ticketeer::core::db::Row;
using ticketeer::core::db::Rows;

void BadRequest(const Callback &callback, const char *msg,
                drogon::HttpStatusCode code = drogon::k400BadRequest);

[[nodiscard]] Rows FetchLookupRows(sqlite3 *db, const char *sql,
                                   const char *log_message);

[[nodiscard]] Rows FetchPriorities(sqlite3 *db);

[[nodiscard]] Rows FetchStatuses(sqlite3 *db);

[[nodiscard]] std::optional<std::string> FetchDefaultStatusId(sqlite3 *db);

[[nodiscard]] std::optional<std::string>
FetchDefaultTicketPriorityId(sqlite3 *db);

[[nodiscard]] std::optional<std::string> FetchDefaultAssignedToId(sqlite3 *db);

[[nodiscard]] int FetchTicketBodyMaxlength(sqlite3 *db);

[[nodiscard]] int FetchTicketActivityBodyMaxlength(sqlite3 *db);

[[nodiscard]] int FetchTicketDueDelta(sqlite3 *db);

[[nodiscard]] std::size_t
FetchActivityAttachmentTotal(sqlite3 *db, const std::string &activity_id);

[[nodiscard]] std::size_t
FetchTicketAttachmentTotal(sqlite3 *db, const std::string &ticket_id);

} // namespace ticketeer::handling::common
