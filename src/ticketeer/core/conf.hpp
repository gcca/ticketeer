#pragma once

#include <string>

namespace ticketeer::core::conf {

struct Settings {
  std::string DB_URL = "db/ticketeer.db";
  std::string UPLOAD_DIR = "db/upload";
};

extern Settings settings;

} // namespace ticketeer::core::conf
