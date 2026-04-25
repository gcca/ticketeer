#pragma once

#include <string>

#include <quill/Logger.h>

namespace ticketeer::core {

void SetupLogging(const std::string &level, bool color = false,
                  bool indent = false);
quill::Logger *logger();

} // namespace ticketeer::core
