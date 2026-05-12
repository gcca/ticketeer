#pragma once

#include <optional>
#include <string>

#include <json/json.h>
#include <libpq-fe.h>

namespace ticketeer::handling::role::common {

inline constexpr int TicketListLimit = 57;

[[nodiscard]] Json::Value
FetchRequesterTicketList(PGconn *pg, const std::string &profile_id,
                         const std::optional<std::string> &search);

[[nodiscard]] Json::Value
FetchSupervisorTicketList(PGconn *pg, const std::optional<std::string> &search);

} // namespace ticketeer::handling::role::common
