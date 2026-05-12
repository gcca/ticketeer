#pragma once

#include <charconv>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <optional>
#include <sstream>
#include <string>

#include <drogon/HttpRequest.h>
#include <drogon/utils/Utilities.h>
#include <json/json.h>
#include <libpq-fe.h>
#include <quill/LogMacros.h>
#include <quill/Logger.h>
#include <sqlite3.h>
#include <zlib.h>

#include "ticketeer/core/conf.hpp"

namespace ticketeer::handling::auth::overlord {
namespace {

constexpr std::string_view kUsernameSuffix = "_overlord";

struct DjangoUser {
  std::uint64_t id;
  std::string username;
};

[[nodiscard]] inline std::string Base64UrlDecode(std::string_view input) {
  std::string b64(input);
  for (auto &c : b64) {
    if (c == '-')
      c = '+';
    else if (c == '_')
      c = '/';
  }
  while (b64.size() % 4 != 0)
    b64 += '=';
  return drogon::utils::base64Decode(b64);
}

[[nodiscard]] inline bool IsExpireDatePast(const std::string &expire_date) {
  std::tm tm = {};
  std::istringstream ss(expire_date);
  ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
  if (ss.fail())
    return true;
  return std::time(nullptr) >= timegm(&tm);
}

[[nodiscard]] inline std::optional<std::string>
FetchDjangoUsername(quill::Logger *logger, sqlite3 *db, std::uint64_t user_id) {
  sqlite3_stmt *stmt = nullptr;
  const char *sql = "SELECT username FROM auth_user WHERE id = ?";

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    LOGJ_DEBUG(logger, "[auth] django user query error", sqlite3_errmsg(db));
    return std::nullopt;
  }

  sqlite3_bind_int64(stmt, 1, static_cast<sqlite3_int64>(user_id));

  std::optional<std::string> username;
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    username = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
  }

  sqlite3_finalize(stmt);
  return username;
}

[[nodiscard]] inline std::optional<DjangoUser>
FetchDjangoSession(quill::Logger *logger, const std::string &session_id) {
  const auto &conn_str = ticketeer::core::conf::settings.OVERLORD_DB_URL;
  LOGJ_DEBUG(logger, "[auth] connecting", conn_str);
  PGconn *pg = PQconnectdb(conn_str.c_str());
  if (PQstatus(pg) != CONNECTION_OK) {
    const std::string error = PQerrorMessage(pg);
    LOGJ_DEBUG(logger, "[auth] connection failed", error);
    PQfinish(pg);
    return std::nullopt;
  }
  LOGJ_DEBUG(logger, "[auth] connected", session_id);

  const char *params[] = {session_id.c_str()};
  PGresult *res = PQexecParams(pg,
                               R"SQL(
SELECT session_data,
       expire_date
  FROM django_session
 WHERE session_key = $1
)SQL",
                               1, nullptr, params, nullptr, nullptr, 0);

  const std::string query_status = PQresStatus(PQresultStatus(res));
  const int rows = PQntuples(res);
  LOGJ_DEBUG(logger, "[auth] query status", query_status);
  LOGJ_DEBUG(logger, "[auth] rows returned", rows);

  std::optional<DjangoUser> user;
  std::uint64_t django_user_id = 0;

  if (PQresultStatus(res) == PGRES_TUPLES_OK && PQntuples(res) == 1) {
    const std::string session_data = PQgetvalue(res, 0, 0);
    const std::string expire_date = PQgetvalue(res, 0, 1);
    LOGJ_DEBUG(logger, "[auth] session data", session_data);
    LOGJ_DEBUG(logger, "[auth] expire date", expire_date);

    if (!IsExpireDatePast(expire_date)) {
      const auto colon = session_data.find(':');
      if (colon != std::string::npos) {
        std::string_view b64_payload(session_data.data(), colon);
        const bool is_compressed = b64_payload[0] == '.';
        if (is_compressed)
          b64_payload.remove_prefix(1);

        std::string payload = Base64UrlDecode(b64_payload);

        std::string json_str;
        if (is_compressed) {
          uLongf dest_len = payload.size() * 8;
          std::string decompressed(dest_len, '\0');
          int ret = uncompress(reinterpret_cast<Bytef *>(decompressed.data()),
                               &dest_len,
                               reinterpret_cast<const Bytef *>(payload.data()),
                               static_cast<uLong>(payload.size()));
          if (ret == Z_OK) {
            decompressed.resize(dest_len);
            json_str = std::move(decompressed);
          }
        } else {
          json_str = std::move(payload);
        }

        if (!json_str.empty()) {
          Json::Value root;
          Json::Reader reader;
          if (reader.parse(json_str, root) && !root["_auth_user_id"].isNull()) {
            const std::string uid_str = root["_auth_user_id"].asString();
            std::from_chars(uid_str.data(), uid_str.data() + uid_str.size(),
                            django_user_id);
          }
        }
      }
    }

    LOGJ_DEBUG(logger, "[auth] user_id", django_user_id);
  } else if (PQresultStatus(res) != PGRES_TUPLES_OK) {
    const std::string error = PQresultErrorMessage(res);
    LOGJ_DEBUG(logger, "[auth] query error", error);
  }

  PQclear(res);
  PQfinish(pg);

  if (django_user_id == 0)
    return std::nullopt;

  user = DjangoUser{django_user_id, std::string{}};
  return user;
}

[[nodiscard]] inline std::optional<std::uint64_t>
ResolveLocalUser(quill::Logger *logger, sqlite3 *db, const DjangoUser &user) {
  const std::string username = user.username + std::string(kUsernameSuffix);
  const std::string provider_id = std::to_string(user.id);

  sqlite3_stmt *stmt = nullptr;
  const char *sql = R"SQL(
INSERT INTO auth_useroauth (user_id, provider_id, provider)
SELECT id, ?, 'overlord'
  FROM auth_user
 WHERE username = ?
ON CONFLICT (provider, provider_id) DO UPDATE SET provider_id = excluded.provider_id
RETURNING user_id
)SQL";

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    LOGJ_DEBUG(logger, "[auth] oauth user query error", sqlite3_errmsg(db));
    return std::nullopt;
  }

  sqlite3_bind_text(stmt, 1, provider_id.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 2, username.c_str(), -1, SQLITE_STATIC);

  std::optional<std::uint64_t> user_id;
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    user_id = static_cast<std::uint64_t>(sqlite3_column_int64(stmt, 0));
  } else {
    LOGJ_DEBUG(logger, "[auth] oauth user query error", sqlite3_errmsg(db));
  }

  sqlite3_finalize(stmt);
  return user_id;
}

} // namespace

[[nodiscard]] static inline std::optional<std::uint64_t>
Authenticate(quill::Logger *logger, sqlite3 *db,
             const drogon::HttpRequestPtr &req) {
  const auto session_id = req->getCookie("sessionid");
  LOGJ_DEBUG(logger, "[auth] session", session_id);
  if (session_id.empty())
    return std::nullopt;

  const auto django_user = FetchDjangoSession(logger, session_id);
  if (!django_user)
    return std::nullopt;

  // Fetch username from local DB using django user id
  const auto username = FetchDjangoUsername(logger, db, django_user->id);
  if (!username)
    return std::nullopt;

  DjangoUser resolved{django_user->id, *username};
  return ResolveLocalUser(logger, db, resolved);
}

} // namespace ticketeer::handling::auth::overlord
