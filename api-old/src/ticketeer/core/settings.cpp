#include "settings.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <random>
#include <string>

namespace {

std::string generate_secret() {
  static constexpr std::string_view alphabet =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

  std::array<std::uint8_t, 32> bytes;
  std::random_device rd;
  std::ranges::generate(bytes, std::ref(rd));

  std::string out;
  out.reserve(43);

  for (int i = 0; i < 32; i += 3) {
    std::uint32_t b = static_cast<std::uint32_t>(bytes[i]) << 16;
    if (i + 1 < 32)
      b |= static_cast<std::uint32_t>(bytes[i + 1]) << 8;
    if (i + 2 < 32)
      b |= static_cast<std::uint32_t>(bytes[i + 2]);

    out += alphabet[(b >> 18) & 0x3F];
    out += alphabet[(b >> 12) & 0x3F];
    if (i + 1 < 32)
      out += alphabet[(b >> 6) & 0x3F];
    if (i + 2 < 32)
      out += alphabet[b & 0x3F];
  }

  return out;
}

} // namespace

namespace ticketeer::core {

Settings settings{.secret = generate_secret()};

}
