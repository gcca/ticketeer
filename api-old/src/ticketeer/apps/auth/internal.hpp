#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <random>
#include <string>
#include <string_view>

#include <crow_all.h>

#include "ticketeer/core/context.hpp"
#include "ticketeer/core/settings.hpp"
#include "ticketeer/db/base.hpp"

namespace ticketeer::apps::auth {

[[nodiscard]] inline std::string GenerateToken() {
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

template <ticketeer::db::Db DB>
[[nodiscard]] crow::response SignIn(const crow::request &req, DB &db,
                                    ticketeer::core::Context &) {
  auto token = GenerateToken();
  auto body = crow::json::load(req.body);
  if (!body || !body.has("username") || !body.has("password"))
    return crow::response(400, "Invalid request body");

  const std::string username = body["username"].s();
  const std::string password = body["password"].s();

  const char *auth_params[] = {username.c_str(), password.c_str()};
  const auto auth =
      db.ExecParams("SELECT id FROM ticketeer.auth_user"
                    " WHERE username = $1 AND password = crypt($2, password)",
                    auth_params, 2);

  if (!auth.ok)
    return crow::response(503, auth.error);
  if (auth.nrows() == 0) {
    crow::json::wvalue err;
    err["error"] = "invalid_credentials";
    err["message"] = "Username or password is incorrect";
    return crow::response(401, err);
  }

  const auto expiry_str =
      std::to_string(ticketeer::core::settings.auth_expiration_seconds);
  const char *session_params[] = {auth.value(0, 0), token.c_str(),
                                  expiry_str.c_str()};
  const auto session = db.ExecParams(
      "INSERT INTO ticketeer.auth_session (user_id, token, expires_at)"
      " VALUES ($1, $2, now() + ($3::integer * interval '1 second'))",
      session_params, 3);

  if (!session.ok)
    return crow::response(503, session.error);

  crow::json::wvalue out;
  out["token"] = std::move(token);
  return crow::response(200, out);
}

} // namespace ticketeer::apps::auth
