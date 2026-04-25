#pragma once

#include <string>

namespace ticketeer::core::conf {

struct Settings {
  std::string DB_URL = "postgres://postgres:postgres@localhost:5436/ticketeer";
  std::string OVERLORD_DB_URL = "postgresql://atlas:atlas@localhost/atlas";
};

extern Settings settings;

} // namespace ticketeer::core::conf
