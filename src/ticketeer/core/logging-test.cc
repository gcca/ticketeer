#include <gtest/gtest.h>

#include "ticketeer/core/logging.hpp"

namespace {

using ticketeer::core::logging::ParseLogLevel;

} // namespace

TEST(ParseLogLevelTest, MapsKnownLevels) {
  EXPECT_EQ(quill::LogLevel::Debug, ParseLogLevel("DEBUG"));
  EXPECT_EQ(quill::LogLevel::Warning, ParseLogLevel("WARNING"));
  EXPECT_EQ(quill::LogLevel::Error, ParseLogLevel("ERROR"));
  EXPECT_EQ(quill::LogLevel::Critical, ParseLogLevel("CRITICAL"));
}

TEST(ParseLogLevelTest, DefaultsToInfo) {
  EXPECT_EQ(quill::LogLevel::Info, ParseLogLevel(""));
  EXPECT_EQ(quill::LogLevel::Info, ParseLogLevel("INFO"));
  EXPECT_EQ(quill::LogLevel::Info, ParseLogLevel("debug"));
}
