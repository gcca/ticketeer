#include <CLI11.hpp>
#include <cstdint>
#include <cstdlib>
#include <print>
#include <random>
#include <string>

#define PBKDF2_SHA256_IMPLEMENTATION
#include <pbkdf2_sha256.h>

#include <sqlite3.h>

int main(int argc, char* argv[]) {
  CLI::App app{"Create a ticketeer user"};

  std::string username;
  std::string password;

  app.add_option("-u,--username", username, "Username")->required();
  app.add_option("-p,--password", password, "Password")->required();

  CLI11_PARSE(app, argc, argv);

  const char* database_url = std::getenv("DATABASE_URL");
  if (!database_url) {
    std::println(stderr, "DATABASE_URL environment variable is not set");
    return 1;
  }

  std::string db_path = database_url;
  if (db_path.starts_with("sqlite:")) {
    db_path = db_path.substr(7);
  }

  sqlite3* db = nullptr;
  sqlite3_stmt* stmt = nullptr;

  int rc = sqlite3_open(db_path.c_str(), &db);
  if (rc != SQLITE_OK) {
    std::println(stderr, "Error opening database: {}", sqlite3_errmsg(db));
    sqlite3_close(db);
    return 1;
  }

  const char* sql = "INSERT INTO auth_user (username, password) VALUES (?, ?)";
  rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
  if (rc != SQLITE_OK) {
    std::println(stderr, "Error preparing statement: {}", sqlite3_errmsg(db));
    sqlite3_close(db);
    return 1;
  }

  std::mt19937_64 rng{std::random_device{}()};
  std::string salt = std::format("{:016x}{:016x}", rng(), rng());

  constexpr uint32_t rounds = 600000;
  constexpr uint32_t dklen = SHA256_DIGESTLEN;
  uint8_t dk[dklen];
  HMAC_SHA256_CTX ctx;
  pbkdf2_sha256(&ctx,
      reinterpret_cast<const uint8_t *>(password.data()),
      static_cast<uint32_t>(password.size()),
      reinterpret_cast<const uint8_t *>(salt.data()),
      static_cast<uint32_t>(salt.size()),
      rounds, dk, dklen);

  std::string hashed = std::format("pbkdf2_sha256${}", rounds);
  hashed += '$' + salt + '$';
  for (uint32_t i = 0; i < dklen; ++i)
    hashed += std::format("{:02x}", dk[i]);

  sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 2, hashed.c_str(), -1, SQLITE_STATIC);

  rc = sqlite3_step(stmt);
  if (rc != SQLITE_DONE) {
    std::println(stderr, "Error creating user: {}", sqlite3_errmsg(db));
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return 1;
  }

  std::println("User '{}' created successfully (ID: {})", username,
               sqlite3_last_insert_rowid(db));

  sqlite3_finalize(stmt);
  sqlite3_close(db);
  return 0;
}
