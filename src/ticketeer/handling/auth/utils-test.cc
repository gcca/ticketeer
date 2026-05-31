#include <memory>
#include <string>

#include <drogon/HttpResponse.h>
#include <gtest/gtest.h>
#include <quill/backend/ThreadUtilities.h>
#include <sqlite3.h>

#include "ticketeer/handling/auth/utils.hpp"

namespace {

using ticketeer::handling::auth::utils::Authenticate;
using ticketeer::handling::auth::utils::FetchUsername;
using ticketeer::handling::auth::utils::LogIn;
using ticketeer::handling::auth::utils::SignOut;
using ticketeer::handling::auth::utils::VerifyPassword;

constexpr char kPasswordHash[] =
    "pbkdf2_sha256$1$salt$"
    "120fb6cffcf8b32c43e7225256c4f837a86548c92ccc35480805987cb70be17b";

struct SqliteDeleter {
  void operator()(sqlite3 *db) const {
    if (db)
      sqlite3_close(db);
  }
};

using SqlitePtr = std::unique_ptr<sqlite3, SqliteDeleter>;

SqlitePtr OpenMemoryDb() {
  sqlite3 *db = nullptr;
  EXPECT_EQ(SQLITE_OK, sqlite3_open(":memory:", &db));
  return SqlitePtr(db);
}

void Exec(sqlite3 *db, const char *sql) {
  char *raw_error = nullptr;
  const int rc = sqlite3_exec(db, sql, nullptr, nullptr, &raw_error);
  std::unique_ptr<char, decltype(&sqlite3_free)> error(raw_error, sqlite3_free);
  ASSERT_EQ(SQLITE_OK, rc) << (error ? error.get() : sqlite3_errmsg(db));
}

void CreateAuthSchema(sqlite3 *db) {
  Exec(db, R"SQL(
CREATE TABLE auth_user (
  id INTEGER PRIMARY KEY,
  username TEXT NOT NULL UNIQUE,
  password TEXT NOT NULL
);

CREATE TABLE auth_session (
  id INTEGER PRIMARY KEY,
  user_id INTEGER NOT NULL,
  token TEXT NOT NULL UNIQUE,
  created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
  expires_at TEXT NOT NULL,
  FOREIGN KEY(user_id) REFERENCES auth_user(id)
);
)SQL");
}

void InsertUser(sqlite3 *db) {
  Exec(db, R"SQL(
INSERT INTO auth_user (id, username, password)
VALUES (7, 'alice', 'pbkdf2_sha256$1$salt$120fb6cffcf8b32c43e7225256c4f837a86548c92ccc35480805987cb70be17b');
)SQL");
}

} // namespace

TEST(VerifyPasswordTest, AcceptsKnownPbkdf2Sha256Vector) {
  EXPECT_TRUE(VerifyPassword("password", kPasswordHash));
  EXPECT_FALSE(VerifyPassword("wrong-password", kPasswordHash));
}

TEST(VerifyPasswordTest, RejectsMalformedHashes) {
  EXPECT_FALSE(VerifyPassword("password", ""));
  EXPECT_FALSE(VerifyPassword("password", "sha256$1$salt$abc"));
  EXPECT_FALSE(VerifyPassword("password", "pbkdf2_sha256$0$salt$abc"));
  EXPECT_FALSE(VerifyPassword("password", "pbkdf2_sha256$1$salt$abc"));
}

TEST(VerifyPasswordTest, ThrowsOnNonNumericRounds) {
  EXPECT_THROW((void)VerifyPassword("password", "pbkdf2_sha256$NaN$salt$abc"),
               std::invalid_argument);
}

TEST(AuthUtilsTest, AuthenticateReturnsUserIdForMatchingCredentials) {
  auto db = OpenMemoryDb();
  ASSERT_NE(nullptr, db.get());
  CreateAuthSchema(db.get());
  InsertUser(db.get());

  const auto user_id = Authenticate(nullptr, db.get(), "alice", "password");
  ASSERT_TRUE(user_id);
  EXPECT_EQ(7U, *user_id);

  EXPECT_FALSE(Authenticate(nullptr, db.get(), "alice", "wrong-password"));
  EXPECT_FALSE(Authenticate(nullptr, db.get(), "missing", "password"));
}

TEST(AuthUtilsTest, LogInFetchUsernameAndSignOutManageSession) {
  auto db = OpenMemoryDb();
  ASSERT_NE(nullptr, db.get());
  CreateAuthSchema(db.get());
  InsertUser(db.get());

  auto resp = drogon::HttpResponse::newHttpResponse();
  ASSERT_TRUE(LogIn(nullptr, db.get(), 7, resp));

  const auto &cookie = resp->getCookie("token");
  ASSERT_TRUE(cookie);
  EXPECT_EQ("token", cookie.key());
  EXPECT_TRUE(cookie.value().starts_with("ticketeer-v1_"));
  EXPECT_EQ(77U, cookie.value().size());
  EXPECT_EQ("/ticketeer", cookie.path());
  EXPECT_TRUE(cookie.isHttpOnly());

  const auto username = FetchUsername(nullptr, db.get(), cookie.value());
  ASSERT_TRUE(username);
  EXPECT_EQ("alice", *username);

  EXPECT_TRUE(SignOut(nullptr, db.get(), cookie.value()));
  EXPECT_FALSE(FetchUsername(nullptr, db.get(), cookie.value()));
}

TEST(AuthUtilsTest, FetchUsernameReturnsFalseForUnknownToken) {
  auto db = OpenMemoryDb();
  ASSERT_NE(nullptr, db.get());
  CreateAuthSchema(db.get());

  EXPECT_FALSE(FetchUsername(nullptr, db.get(), "ticketeer-v1_nonexistent"));
}
