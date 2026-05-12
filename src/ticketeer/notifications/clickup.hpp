#pragma once

#include <map>
#include <string>

namespace ticketeer::notifications::clickup {

using Row = std::map<std::string, std::string>;

void NotifyTicketCreated(const Row &ticket, const std::string &requester_name);

} // namespace ticketeer::notifications::clickup
