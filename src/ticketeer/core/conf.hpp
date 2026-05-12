#pragma once

#include <string>

namespace ticketeer::core::conf {

struct Settings {
  std::string DB_URL = "db/ticketeer.db";
  std::string OVERLORD_DB_URL = "";
  std::string UPLOAD_DIR = "upload";
};

extern Settings settings;

} // namespace ticketeer::core::conf
