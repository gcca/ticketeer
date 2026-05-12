#pragma once

#include <chrono>
#include <string>

namespace ticketeer::core::conf {

struct Settings {
  std::string DB_PATH = "data/ticketeer.db";
  std::string UPLOAD_DIR = "data/upload";
  std::string TZ = "UTC";
  std::string SENTRY_DSN;
  std::string CLICKUP_API_TOKEN;
  std::string CLICKUP_WORKSPACEID;
  std::string CLICKUP_TICKETEER_CHANNELID;
  std::string PULSE_DB_PATH = "data/pulse.db";
  std::chrono::seconds PULSE_INTERVAL = std::chrono::minutes(1);
};

extern Settings settings;

} // namespace ticketeer::core::conf
