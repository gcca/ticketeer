#include <gtest/gtest.h>

#include "ticketeer/core/options.hpp"

namespace {

using ticketeer::core::options::InitServerOptions;
using ticketeer::core::options::ServerOptions;

} // namespace

TEST(ServerOptionsTest, Defaults) {
  char prog[] = "ticketeer";
  char *argv[] = {prog};
  const auto opts = InitServerOptions(1, argv);
  EXPECT_EQ("0.0.0.0", opts.bind);
  EXPECT_EQ(5521U, opts.port);
  EXPECT_EQ("INFO", opts.log_level);
}

TEST(ServerOptionsTest, ParsesBind) {
  char prog[] = "ticketeer";
  char flag[] = "--bind";
  char val[] = "127.0.0.1";
  char *argv[] = {prog, flag, val};
  const auto opts = InitServerOptions(3, argv);
  EXPECT_EQ("127.0.0.1", opts.bind);
}

TEST(ServerOptionsTest, ParsesPort) {
  char prog[] = "ticketeer";
  char flag[] = "--port";
  char val[] = "8080";
  char *argv[] = {prog, flag, val};
  const auto opts = InitServerOptions(3, argv);
  EXPECT_EQ(static_cast<std::uint16_t>(8080), opts.port);
}

TEST(ServerOptionsTest, ParsesLogLevel) {
  char prog[] = "ticketeer";
  char flag[] = "--log_level";
  char val[] = "DEBUG";
  char *argv[] = {prog, flag, val};
  const auto opts = InitServerOptions(3, argv);
  EXPECT_EQ("DEBUG", opts.log_level);
}

TEST(ServerOptionsTest, ParsesAllOptions) {
  char prog[] = "ticketeer";
  char b_flag[] = "--bind";
  char b_val[] = "192.168.1.1";
  char p_flag[] = "--port";
  char p_val[] = "9000";
  char l_flag[] = "--log_level";
  char l_val[] = "WARNING";
  char *argv[] = {prog, b_flag, b_val, p_flag, p_val, l_flag, l_val};
  const auto opts = InitServerOptions(7, argv);
  EXPECT_EQ("192.168.1.1", opts.bind);
  EXPECT_EQ(static_cast<std::uint16_t>(9000), opts.port);
  EXPECT_EQ("WARNING", opts.log_level);
}
