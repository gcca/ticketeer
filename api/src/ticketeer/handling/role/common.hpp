#pragma once

#include <optional>
#include <string>

#include <json/json.h>
#include <libpq-fe.h>
#include <quill/Logger.h>

namespace ticketeer::api::role::common {

inline constexpr int TicketListLimit = 57;

[[nodiscard]] Json::Value
FetchRequesterTicketList(quill::Logger *logger, PGconn *pg,
                         const std::string &profile_id,
                         const std::optional<std::string> &search);

[[nodiscard]] Json::Value
FetchSupervisorTicketList(quill::Logger *logger, PGconn *pg,
                          const std::optional<std::string> &search);

} // namespace ticketeer::api::role::common
