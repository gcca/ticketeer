#include "conf.hpp"

#include <cstdlib>
#include <filesystem>

namespace ticketeer::core::conf {

[[nodiscard]] static Settings InitSettings() {
  Settings settings;

  if (const auto *DB_PATH = std::getenv("DB_PATH")) {
    settings.DB_PATH = DB_PATH;
  }

  if (const auto *UPLOAD_DIR = std::getenv("UPLOAD_DIR")) {
    settings.UPLOAD_DIR = UPLOAD_DIR;
  }

  if (const auto *TZ = std::getenv("TZ")) {
    settings.TZ = TZ;
  }

  if (const auto *SENTRY_DSN = std::getenv("SENTRY_DSN")) {
    settings.SENTRY_DSN = SENTRY_DSN;
  }

  if (const auto *CLICKUP_API_TOKEN = std::getenv("CLICKUP_API_TOKEN")) {
    settings.CLICKUP_API_TOKEN = CLICKUP_API_TOKEN;
  }

  if (const auto *CLICKUP_WORKSPACEID = std::getenv("CLICKUP_WORKSPACEID")) {
    settings.CLICKUP_WORKSPACEID = CLICKUP_WORKSPACEID;
  }

  if (const auto *CLICKUP_TICKETEER_CHANNELID =
          std::getenv("CLICKUP_TICKETEER_CHANNELID")) {
    settings.CLICKUP_TICKETEER_CHANNELID = CLICKUP_TICKETEER_CHANNELID;
  }

  if (const auto *PULSE_DB_PATH = std::getenv("PULSE_DB_PATH")) {
    settings.PULSE_DB_PATH = PULSE_DB_PATH;
  }

  if (const auto *PULSE_INTERVAL = std::getenv("PULSE_INTERVAL")) {
    if (const long seconds = std::strtol(PULSE_INTERVAL, nullptr, 10);
        seconds > 0) {
      settings.PULSE_INTERVAL = std::chrono::seconds(seconds);
    }
  }

  std::string db_path = settings.DB_PATH;
  if (const std::filesystem::path p(db_path); p.has_parent_path()) {
    std::filesystem::create_directories(p.parent_path());
  }

  return settings;
}

Settings settings = InitSettings();

} // namespace ticketeer::core::conf
