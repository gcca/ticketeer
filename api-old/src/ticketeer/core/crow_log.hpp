#pragma once

#include <crow_all.h>
#include <quill/LogMacros.h>

#include "log.hpp"

namespace ticketeer::core {

class CrowLogHandler : public crow::ILogHandler {
public:
  void log(const std::string &message, crow::LogLevel level) override {
    auto *l = logger();
    switch (level) {
    case crow::LogLevel::Debug:
      LOG_DEBUG(l, "{}", message);
      break;
    case crow::LogLevel::Info:
      LOG_INFO(l, "{}", message);
      break;
    case crow::LogLevel::Warning:
      LOG_WARNING(l, "{}", message);
      break;
    case crow::LogLevel::Error:
      LOG_ERROR(l, "{}", message);
      break;
    case crow::LogLevel::Critical:
      LOG_CRITICAL(l, "{}", message);
      break;
    }
  }
};

} // namespace ticketeer::core
