#include "conf.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string_view>

namespace ticketeer::core::conf {

[[nodiscard]] static std::string DetectServerTimezone() {
  if (const auto *TZ = std::getenv("TZ"); TZ && *TZ)
    return TZ;

  if (std::ifstream timezone_file{"/etc/timezone"}) {
    std::string timezone;
    std::getline(timezone_file, timezone);
    if (!timezone.empty())
      return timezone;
  }

  std::error_code ec;
  const auto localtime = std::filesystem::read_symlink("/etc/localtime", ec);
  if (!ec) {
    const auto path = localtime.generic_string();
    constexpr std::string_view zoneinfo = "/zoneinfo/";
    if (const auto pos = path.find(zoneinfo); pos != std::string::npos)
      return path.substr(pos + zoneinfo.size());
  }

  return "UTC";
}

[[nodiscard]] static Settings InitSettings() {
  Settings settings;
  settings.SERVER_TIMEZONE = DetectServerTimezone();

  if (const auto *DB_URL = std::getenv("DB_URL")) {
    settings.DB_URL = DB_URL;
  }

  if (const auto *UPLOAD_DIR = std::getenv("UPLOAD_DIR")) {
    settings.UPLOAD_DIR = UPLOAD_DIR;
  }

  std::string db_path = settings.DB_URL;
  if (db_path.starts_with("sqlite:")) db_path = db_path.substr(7);
  if (const std::filesystem::path p(db_path); p.has_parent_path()) {
    std::filesystem::create_directories(p.parent_path());
  }

  return settings;
}

Settings settings = InitSettings();

} // namespace ticketeer::core::conf
