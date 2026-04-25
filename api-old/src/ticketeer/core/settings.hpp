#pragma once

#include <cstdint>
#include <string>

namespace ticketeer::core {

struct Settings {
  std::string secret;
  std::string db_conn = "host=localhost dbname=ticketeer";
  std::uint32_t auth_expiration_seconds = 90 * 24 * 60 * 60;
};

extern Settings settings;

} // namespace ticketeer::core
