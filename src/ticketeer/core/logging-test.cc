#include <gtest/gtest.h>

#include "ticketeer/core/logging.hpp"

namespace {

using ticketeer::core::logging::ParseLogLevel;

} // namespace

TEST(ParseLogLevelTest, MapsKnownLevels) {
  EXPECT_EQ(trantor::Logger::kDebug, ParseLogLevel("DEBUG"));
  EXPECT_EQ(trantor::Logger::kWarn, ParseLogLevel("WARNING"));
  EXPECT_EQ(trantor::Logger::kError, ParseLogLevel("ERROR"));
  EXPECT_EQ(trantor::Logger::kFatal, ParseLogLevel("CRITICAL"));
}

TEST(ParseLogLevelTest, DefaultsToInfo) {
  EXPECT_EQ(trantor::Logger::kInfo, ParseLogLevel(""));
  EXPECT_EQ(trantor::Logger::kInfo, ParseLogLevel("INFO"));
  EXPECT_EQ(trantor::Logger::kInfo, ParseLogLevel("debug"));
}
