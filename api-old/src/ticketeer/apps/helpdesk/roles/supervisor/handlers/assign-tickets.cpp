#include "ticketeer/apps/helpdesk/roles/supervisor/handlers/assign-tickets.hpp"

namespace ticketeer::apps::helpdesk::roles::supervisor::handlers::internal {

[[nodiscard]] std::optional<const char *>
GetTicketIdValue(const crow::json::rvalue &item, std::string &s) {
  if (not item.has("ticket_id"))
    return std::nullopt;

  auto &tid = item["ticket_id"];

  if ((tid.t() != crow::json::type::Number) or (tid.i() <= 0))
    return std::nullopt;

  s = static_cast<std::string>(tid);

  return s.c_str();
}

[[nodiscard]] std::optional<const char *>
GetTechnicianIdValue(const crow::json::rvalue &item, std::string &s) {
  if (not item.has("technician_id"))
    return std::nullopt;

  auto &tid = item["technician_id"];

  if (tid.t() == crow::json::type::Null)
    return nullptr;

  if ((tid.t() != crow::json::type::Number) or (tid.i() <= 0))
    return std::nullopt;

  s = static_cast<std::string>(tid);

  return s.c_str();
}

} // namespace ticketeer::apps::helpdesk::roles::supervisor::handlers::internal
