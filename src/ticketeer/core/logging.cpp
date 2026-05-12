#include "logging.hpp"

namespace ticketeer::core::logging {

trantor::Logger::LogLevel ParseLogLevel(const std::string &level) {
  if (level == "DEBUG")
    return trantor::Logger::kDebug;
  if (level == "WARNING")
    return trantor::Logger::kWarn;
  if (level == "ERROR")
    return trantor::Logger::kError;
  if (level == "CRITICAL")
    return trantor::Logger::kFatal;
  return trantor::Logger::kInfo;
}

} // namespace ticketeer::core::logging
