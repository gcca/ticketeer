#include <gtest/gtest.h>

#include "ticketeer/core/conf.hpp"

TEST(SettingsTest, Defaults) {
  ticketeer::core::conf::Settings s;
  EXPECT_EQ("data/ticketeer.db", s.DB_PATH);
  EXPECT_EQ("data/upload", s.UPLOAD_DIR);
  EXPECT_EQ("UTC", s.TZ);
  EXPECT_TRUE(s.SENTRY_DSN.empty());
}
