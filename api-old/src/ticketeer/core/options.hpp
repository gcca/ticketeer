#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>

namespace ticketeer::core {

struct OptionsParseError : std::runtime_error {
  int exit_code;
  explicit OptionsParseError(const std::string &msg, int code)
      : std::runtime_error{msg}, exit_code{code} {}
};

struct Options {
  std::uint16_t port = 8000;
  bool nothreaded = false;
  bool noconcurrency = false;
  std::string log_level = "warning";
  bool log_color = false;
  bool log_format = false;

  static Options Parse(int argc, char **argv);
};

} // namespace ticketeer::core
