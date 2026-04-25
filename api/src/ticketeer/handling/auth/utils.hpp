#pragma once

#include <charconv>
#include <cstdint>
#include <cstring>
#include <optional>
#include <string>

#include <libpq-fe.h>
#include <quill/LogMacros.h>
#include <quill/Logger.h>

namespace ticketeer::api::auth::utils {

[[nodiscard]] static inline std::optional<std::uint64_t>
Authenticate(quill::Logger *logger, PGconn *pg, const std::string &username,
             const std::string &password) {
  const char *params[] = {username.c_str(), password.c_str()};
  PGresult *res = PQexecParams(pg,
                               R"SQL(
SELECT id
  FROM auth."user"
 WHERE username = $1
   AND password = crypt($2, password)
)SQL",
                               2, nullptr, params, nullptr, nullptr, 0);

  std::optional<std::uint64_t> user_id;

  if (PQresultStatus(res) == PGRES_TUPLES_OK && PQntuples(res) == 1) {
    std::uint64_t id = 0;
    const char *val = PQgetvalue(res, 0, 0);
    std::from_chars(val, val + std::strlen(val), id);
    user_id = id;
  } else if (PQresultStatus(res) != PGRES_TUPLES_OK) {
    LOGJ_DEBUG(logger, "[auth] query error", PQresultErrorMessage(res));
  }

  PQclear(res);
  return user_id;
}

struct Session {
  std::string token;
  std::string expiry;
};

[[nodiscard]] static inline std::optional<Session>
LogIn(quill::Logger *logger, PGconn *pg, std::uint64_t user_id) {
  const std::string uid_str = std::to_string(user_id);
  const char *params[] = {uid_str.c_str()};
  PGresult *res = PQexecParams(pg,
                               R"SQL(
INSERT INTO auth.session (user_id, token, expires_at)
VALUES ($1,
        'ticketeer-v1_' || encode(gen_random_bytes(32), 'hex'),
        now() + interval '1 month')
RETURNING token,
          expires_at
)SQL",
                               1, nullptr, params, nullptr, nullptr, 0);

  std::optional<Session> session;

  if (PQresultStatus(res) == PGRES_TUPLES_OK && PQntuples(res) == 1) {
    session = Session{PQgetvalue(res, 0, 0), PQgetvalue(res, 0, 1)};
  } else {
    LOGJ_DEBUG(logger, "[auth] insert error", PQresultErrorMessage(res));
  }

  PQclear(res);
  return session;
}

[[nodiscard]] static inline std::optional<std::string>
FetchUsername(quill::Logger *logger, PGconn *pg, const std::string &token) {
  const char *params[] = {token.c_str()};
  PGresult *res = PQexecParams(pg,
                               R"SQL(
SELECT u.username
  FROM auth.session s
  JOIN auth."user" u
    ON u.id = s.user_id
 WHERE s.token = $1
   AND s.expires_at > now()
)SQL",
                               1, nullptr, params, nullptr, nullptr, 0);

  std::optional<std::string> username;

  if (PQresultStatus(res) == PGRES_TUPLES_OK && PQntuples(res) == 1) {
    username = PQgetvalue(res, 0, 0);
  } else if (PQresultStatus(res) != PGRES_TUPLES_OK) {
    LOGJ_DEBUG(logger, "[auth] query error", PQresultErrorMessage(res));
  }

  PQclear(res);
  return username;
}

[[nodiscard]] static inline bool SignOut(quill::Logger *logger, PGconn *pg,
                                         const std::string &token) {
  const char *params[] = {token.c_str()};
  PGresult *res = PQexecParams(pg,
                               R"SQL(
DELETE FROM auth.session
 WHERE token = $1
)SQL",
                               1, nullptr, params, nullptr, nullptr, 0);

  bool success = PQresultStatus(res) == PGRES_COMMAND_OK &&
                 std::strcmp(PQcmdTuples(res), "1") == 0;

  if (!success) {
    LOGJ_DEBUG(logger, "[auth] logout error", PQresultErrorMessage(res));
  }

  PQclear(res);
  return success;
}

} // namespace ticketeer::api::auth::utils
