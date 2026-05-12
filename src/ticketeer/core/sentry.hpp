#pragma once

#include "conf.hpp"
#include "options.hpp"

namespace ticketeer::core::sentry {

[[nodiscard]] bool Init(const conf::Settings &settings);

void StartupEvent(const options::Options &options,
                  const conf::Settings &settings);

void Close();

} // namespace ticketeer::core::sentry
