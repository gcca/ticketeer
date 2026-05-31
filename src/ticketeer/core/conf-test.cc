#include <gtest/gtest.h>

#include "ticketeer/core/conf.hpp"

TEST(SettingsTest, Defaults) {
  ticketeer::core::conf::Settings s;
  EXPECT_EQ("data/ticketeer.db", s.DB_URL);
  EXPECT_EQ("data/upload", s.UPLOAD_DIR);
  EXPECT_EQ("UTC", s.SERVER_TIMEZONE);
}
