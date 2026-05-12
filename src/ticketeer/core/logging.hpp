#pragma once

#include <string>

#include <trantor/utils/Logger.h>

namespace ticketeer::core::logging {

[[nodiscard]] trantor::Logger::LogLevel ParseLogLevel(const std::string &level);

} // namespace ticketeer::core::logging
