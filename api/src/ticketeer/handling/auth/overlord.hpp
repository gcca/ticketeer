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
#include <zlib.h>

#include "ticketeer/core/conf.hpp"

namespace ticketeer::api::auth::overlord {
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
FetchDjangoUsername(quill::Logger *logger, PGconn *pg, std::uint64_t user_id) {
  const std::string user_id_str = std::to_string(user_id);
  const char *params[] = {user_id_str.c_str()};
  PGresult *res = PQexecParams(pg,
                               R"SQL(
SELECT username
  FROM auth_user
 WHERE id = $1::bigint
)SQL",
                               1, nullptr, params, nullptr, nullptr, 0);

  std::optional<std::string> username;

  if (PQresultStatus(res) == PGRES_TUPLES_OK && PQntuples(res) == 1) {
    username = PQgetvalue(res, 0, 0);
  } else if (PQresultStatus(res) != PGRES_TUPLES_OK) {
    const std::string error = PQresultErrorMessage(res);
    LOGJ_DEBUG(logger, "[auth] django user query error", error);
  }

  PQclear(res);
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
    if (django_user_id != 0) {
      const auto username = FetchDjangoUsername(logger, pg, django_user_id);
      if (username)
        user = DjangoUser{django_user_id, *username};
    }
  } else if (PQresultStatus(res) != PGRES_TUPLES_OK) {
    const std::string error = PQresultErrorMessage(res);
    LOGJ_DEBUG(logger, "[auth] query error", error);
  }

  PQclear(res);
  PQfinish(pg);
  return user;
}

[[nodiscard]] inline std::optional<std::uint64_t>
ResolveLocalUser(quill::Logger *logger, PGconn *pg, const DjangoUser &user) {
  const std::string username = user.username + std::string(kUsernameSuffix);
  const std::string provider_id = std::to_string(user.id);
  const char *params[] = {username.c_str(), provider_id.c_str()};
  PGresult *res = PQexecParams(pg,
                               R"SQL(
WITH existing AS (
  SELECT user_id
    FROM auth.useroauth
   WHERE provider = 'overlord'::auth.provider
     AND provider_id = $2::bigint
), updated_existing AS (
  UPDATE auth."user" u
     SET username = $1
    FROM existing e
   WHERE u.id = e.user_id
  RETURNING u.id AS user_id
), inserted_user AS (
  INSERT INTO auth."user" (username, password)
  SELECT $1,
         crypt(encode(gen_random_bytes(32), 'hex'), gen_salt('bf'))
    WHERE NOT EXISTS (SELECT 1 FROM existing)
  ON CONFLICT (username) DO UPDATE SET username = EXCLUDED.username
  RETURNING id
), inserted_oauth AS (
  INSERT INTO auth.useroauth (user_id, provider_id, provider)
  SELECT id,
         $2::bigint,
         'overlord'::auth.provider
    FROM inserted_user
  RETURNING user_id
)
SELECT user_id FROM updated_existing
 UNION ALL
 SELECT user_id FROM inserted_oauth
 LIMIT 1
)SQL",
                               2, nullptr, params, nullptr, nullptr, 0);

  std::optional<std::uint64_t> user_id;

  if (PQresultStatus(res) == PGRES_TUPLES_OK && PQntuples(res) == 1) {
    std::uint64_t id = 0;
    const char *val = PQgetvalue(res, 0, 0);
    std::from_chars(val, val + std::strlen(val), id);
    user_id = id;
  } else {
    LOGJ_DEBUG(logger, "[auth] oauth user query error",
               PQresultErrorMessage(res));
  }

  PQclear(res);
  return user_id;
}

} // namespace

[[nodiscard]] static inline std::optional<std::uint64_t>
Authenticate(quill::Logger *logger, PGconn *pg,
             const drogon::HttpRequestPtr &req) {
  const auto session_id = req->getCookie("sessionid");
  LOGJ_DEBUG(logger, "[auth] session", session_id);
  if (session_id.empty())
    return std::nullopt;

  const auto django_user = FetchDjangoSession(logger, session_id);
  if (!django_user)
    return std::nullopt;

  return ResolveLocalUser(logger, pg, *django_user);
}

} // namespace ticketeer::api::auth::overlord
