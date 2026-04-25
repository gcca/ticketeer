#pragma once

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include <crow_all.h>

namespace ticketeer::core {

struct PageParams {
  int page;
  int page_size;

  [[nodiscard]] int limit() const noexcept { return page_size; }
  [[nodiscard]] int offset() const noexcept { return (page - 1) * page_size; }

  [[nodiscard]] static PageParams FromRequest(const crow::request &req,
                                              int default_size = 20,
                                              int max_size = 100) {
    const char *ps = req.url_params.get("page");
    const char *ss = req.url_params.get("page_size");
    const int page = ps ? std::max(1, std::atoi(ps)) : 1;
    const int page_size =
        ss ? std::clamp(std::atoi(ss), 1, max_size) : default_size;
    return {page, page_size};
  }
};

struct KeysetParams {
  int limit;
  std::string cursor;

  [[nodiscard]] bool has_cursor() const noexcept { return !cursor.empty(); }

  [[nodiscard]] static KeysetParams FromRequest(const crow::request &req,
                                                int default_limit = 20) {
    const char *ls = req.url_params.get("limit");
    const char *cs = req.url_params.get("cursor");
    const int limit = ls ? std::clamp(std::atoi(ls), 1, 100) : default_limit;
    return {limit, cs ? std::string{cs} : std::string{}};
  }
};

[[nodiscard]] inline std::string EncodeCursor(long long id,
                                              std::string_view created_at) {
  static constexpr std::string_view kTable =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
  const std::string raw = std::to_string(id) + "," + std::string{created_at};
  std::string out;
  out.reserve(((raw.size() + 2) / 3) * 4);
  const auto *data = reinterpret_cast<const unsigned char *>(raw.data());
  for (std::size_t i = 0; i < raw.size(); i += 3) {
    const unsigned b0 = data[i];
    const unsigned b1 = i + 1 < raw.size() ? data[i + 1] : 0u;
    const unsigned b2 = i + 2 < raw.size() ? data[i + 2] : 0u;
    out += kTable[b0 >> 2];
    out += kTable[((b0 & 3) << 4) | (b1 >> 4)];
    if (i + 1 < raw.size())
      out += kTable[((b1 & 15) << 2) | (b2 >> 6)];
    if (i + 2 < raw.size())
      out += kTable[b2 & 63];
  }
  return out;
}

[[nodiscard]] inline std::optional<std::pair<long long, std::string>>
DecodeCursor(std::string_view cursor) {
  static constexpr auto kDecode = [] {
    std::array<int, 128> t{};
    t.fill(-1);
    constexpr std::string_view kTable =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
    for (int i = 0; i < 64; ++i)
      t[static_cast<unsigned char>(kTable[i])] = i;
    return t;
  }();

  std::string raw;
  raw.reserve(cursor.size() * 3 / 4);
  int bits = 0, acc = 0;
  for (const char c : cursor) {
    const auto uc = static_cast<unsigned char>(c);
    if (uc >= 128 || kDecode[uc] < 0)
      return std::nullopt;
    acc = (acc << 6) | kDecode[uc];
    bits += 6;
    if (bits >= 8) {
      bits -= 8;
      raw += static_cast<char>((acc >> bits) & 0xFF);
    }
  }

  const auto sep = raw.find(',');
  if (sep == std::string::npos)
    return std::nullopt;
  try {
    return std::pair{std::stoll(raw.substr(0, sep)), raw.substr(sep + 1)};
  } catch (...) {
    return std::nullopt;
  }
}

} // namespace ticketeer::core
