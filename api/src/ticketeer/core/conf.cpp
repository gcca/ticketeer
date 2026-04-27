#include "conf.hpp"

#include <cstdlib>

namespace ticketeer::core::conf {

[[nodiscard]] static Settings InitSettings() {
  Settings settings;

  if (const auto *DB_URL = std::getenv("DB_URL")) {
    settings.DB_URL = DB_URL;
  }

  if (const auto *OVERLORD_DB_URL = std::getenv("OVERLORD_DB_URL")) {
    settings.OVERLORD_DB_URL = OVERLORD_DB_URL;
  }

  if (const auto *UPLOAD_DIR = std::getenv("UPLOAD_DIR")) {
    settings.UPLOAD_DIR = UPLOAD_DIR;
  }

  return settings;
}

Settings settings = InitSettings();

} // namespace ticketeer::core::conf
