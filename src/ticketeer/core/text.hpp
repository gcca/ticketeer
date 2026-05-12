#pragma once

#include <cstddef>
#include <string>

namespace ticketeer::core::text {

[[nodiscard]] inline std::size_t Utf8Length(const std::string &str) {
  std::size_t length = 0;
  for (std::size_t i = 0; i < str.size();) {
    const auto ch = static_cast<unsigned char>(str[i]);
    if ((ch & 0x80) == 0) {
      ++i;
    } else if ((ch & 0xE0) == 0xC0) {
      i += 2;
    } else if ((ch & 0xF0) == 0xE0) {
      i += 3;
    } else if ((ch & 0xF8) == 0xF0) {
      i += 4;
    } else {
      ++i;
    }
    ++length;
  }
  return length;
}

} // namespace ticketeer::core::text
