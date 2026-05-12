#pragma once

#include <cstdint>
#include <format>
#include <optional>
#include <random>
#include <string>

#include <drogon/HttpResponse.h>
#include <pbkdf2_sha256.h>
#include <quill/LogMacros.h>
#include <quill/Logger.h>
#include <sqlite3.h>
#include <trantor/utils/Date.h>

namespace ticketeer::handling::auth::utils {

[[nodiscard]] static inline bool VerifyPassword(const std::string &password,
                                                const std::string &stored) {
  auto p1 = stored.find('$');
  if (p1 == std::string::npos)
    return false;
  auto p2 = stored.find('$', p1 + 1);
  if (p2 == std::string::npos)
    return false;
  auto p3 = stored.find('$', p2 + 1);
  if (p3 == std::string::npos)
    return false;

  const std::string algo = stored.substr(0, p1);
  if (algo != "pbkdf2_sha256")
    return false;

  const uint32_t rounds =
      static_cast<uint32_t>(std::stoul(stored.substr(p1 + 1, p2 - p1 - 1)));
  const std::string salt = stored.substr(p2 + 1, p3 - p2 - 1);
  const std::string hex_dk = stored.substr(p3 + 1);

  constexpr uint32_t dklen = SHA256_DIGESTLEN;
  uint8_t dk[dklen];
  HMAC_SHA256_CTX ctx;
  pbkdf2_sha256(&ctx, reinterpret_cast<const uint8_t *>(password.data()),
                static_cast<uint32_t>(password.size()),
                reinterpret_cast<const uint8_t *>(salt.data()),
                static_cast<uint32_t>(salt.size()), rounds, dk, dklen);

  if (hex_dk.size() != dklen * 2)
    return false;
  for (uint32_t i = 0; i < dklen; ++i) {
    char buf[3];
    std::snprintf(buf, sizeof(buf), "%02x", dk[i]);
    if (hex_dk[2 * i] != buf[0] || hex_dk[2 * i + 1] != buf[1])
      return false;
  }
  return true;
}

[[nodiscard]] static inline std::optional<std::uint64_t>
Authenticate(quill::Logger *logger, sqlite3 *db, const std::string &username,
             const std::string &password) {
  sqlite3_stmt *stmt = nullptr;
  const char *sql = R"SQL(
SELECT id, password
  FROM auth_user
 WHERE username = ?
)SQL";

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    LOGJ_DEBUG(logger, "[auth] query error", sqlite3_errmsg(db));
    return std::nullopt;
  }

  sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);

  std::optional<std::uint64_t> user_id;
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    const auto stored =
        reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
    if (stored && VerifyPassword(password, stored)) {
      user_id = static_cast<std::uint64_t>(sqlite3_column_int64(stmt, 0));
    }
  }

  sqlite3_finalize(stmt);
  return user_id;
}

[[nodiscard]] static inline bool LogIn(quill::Logger *logger, sqlite3 *db,
                                       std::uint64_t user_id,
                                       const drogon::HttpResponsePtr &resp) {
  std::mt19937_64 rng{std::random_device{}()};
  std::string token = "ticketeer-v1_";
  for (int i = 0; i < 4; ++i)
    token += std::format("{:016x}", rng());

  sqlite3_stmt *stmt = nullptr;
  const char *sql = R"SQL(
INSERT INTO auth_session (user_id, token, expires_at)
VALUES (?, ?, datetime('now', '+1 month'))
RETURNING token, expires_at
)SQL";

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    LOGJ_DEBUG(logger, "[auth] insert error", sqlite3_errmsg(db));
    return false;
  }

  sqlite3_bind_int64(stmt, 1, static_cast<sqlite3_int64>(user_id));
  sqlite3_bind_text(stmt, 2, token.c_str(), -1, SQLITE_STATIC);

  bool ok = false;
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    const std::string tok =
        reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
    const std::string expiry =
        reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
    drogon::Cookie c("token", tok);
    c.setHttpOnly(true);
    c.setPath("/ticketeer");
    c.setExpiresDate(trantor::Date::fromDbStringLocal(expiry));
    resp->addCookie(std::move(c));
    ok = true;
  } else {
    LOGJ_DEBUG(logger, "[auth] insert error", sqlite3_errmsg(db));
  }

  sqlite3_finalize(stmt);
  return ok;
}

[[nodiscard]] static inline std::optional<std::string>
FetchUsername(quill::Logger *logger, sqlite3 *db, const std::string &token) {
  sqlite3_stmt *stmt = nullptr;
  const char *sql = R"SQL(
SELECT u.username
  FROM auth_session s
  JOIN auth_user u
    ON u.id = s.user_id
 WHERE s.token = ?
   AND s.expires_at > datetime('now')
)SQL";

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    LOGJ_DEBUG(logger, "[auth] query error", sqlite3_errmsg(db));
    return std::nullopt;
  }

  sqlite3_bind_text(stmt, 1, token.c_str(), -1, SQLITE_STATIC);

  std::optional<std::string> username;
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    username = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
  }

  sqlite3_finalize(stmt);
  return username;
}

[[nodiscard]] static inline bool SignOut(quill::Logger *logger, sqlite3 *db,
                                         const std::string &token) {
  sqlite3_stmt *stmt = nullptr;
  const char *sql = "DELETE FROM auth_session WHERE token = ?";

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    LOGJ_DEBUG(logger, "[auth] logout error", sqlite3_errmsg(db));
    return false;
  }

  sqlite3_bind_text(stmt, 1, token.c_str(), -1, SQLITE_STATIC);

  bool success = sqlite3_step(stmt) == SQLITE_DONE && sqlite3_changes(db) == 1;

  if (!success) {
    LOGJ_DEBUG(logger, "[auth] logout error", sqlite3_errmsg(db));
  }

  sqlite3_finalize(stmt);
  return success;
}

} // namespace ticketeer::handling::auth::utils
