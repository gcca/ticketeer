#pragma once

#include <string>

namespace ticketeer::core::conf {

struct Settings {
  std::string DB_URL = "data/ticketeer.db";
  std::string UPLOAD_DIR = "data/upload";
  std::string SERVER_TIMEZONE = "UTC";
};

extern Settings settings;

} // namespace ticketeer::core::conf
