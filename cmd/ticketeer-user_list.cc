#include <CLI11.hpp>
#include <cstdlib>
#include <print>
#include <string>

#include <sqlite3.h>

int main(int argc, char* argv[]) {
  CLI::App app{"List ticketeer users"};
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
  int rc = sqlite3_open_v2(db_path.c_str(), &db, SQLITE_OPEN_READONLY, nullptr);
  if (rc != SQLITE_OK) {
    std::println(stderr, "Error opening database: {}", sqlite3_errmsg(db));
    sqlite3_close(db);
    return 1;
  }

  const char* sql =
      "SELECT u.id, u.username, u.name, u.email, p.role, u.created_at "
      "FROM auth_user u "
      "LEFT JOIN helpdesk_profile p ON p.user_id = u.id "
      "ORDER BY u.id";

  sqlite3_stmt* stmt = nullptr;
  rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
  if (rc != SQLITE_OK) {
    std::println(stderr, "Error preparing statement: {}", sqlite3_errmsg(db));
    sqlite3_close(db);
    return 1;
  }

  std::println("{:<4} {:<20} {:<24} {:<32} {:<15} {}", "ID", "Username", "Name", "Email", "Role", "Created at");
  std::println("{}", std::string(105, '-'));

  while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
    int         id         = sqlite3_column_int(stmt, 0);
    const char* username   = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
    const char* name       = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
    const char* email      = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
    const char* role       = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
    const char* created_at = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
    std::println("{:<4} {:<20} {:<24} {:<32} {:<15} {}",
        id,
        username   ? username   : "",
        name       ? name       : "",
        email      ? email      : "",
        role       ? role       : "",
        created_at ? created_at : "");
  }

  if (rc != SQLITE_DONE) {
    std::println(stderr, "Error reading rows: {}", sqlite3_errmsg(db));
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return 1;
  }

  sqlite3_finalize(stmt);
  sqlite3_close(db);
  return 0;
}
