#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <print>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include <sqlite3.h>
#include <unistd.h>

namespace {

constexpr std::string_view kUpMarker = "-- migrate:up";
constexpr std::string_view kDownMarker = "-- migrate:down";

struct Migration {
  std::string version;
  std::filesystem::path path;
};

[[nodiscard]] std::string ReadFile(const std::filesystem::path &path) {
  std::ifstream file(path, std::ios::binary);
  std::ostringstream buffer;
  buffer << file.rdbuf();
  return buffer.str();
}

[[nodiscard]] bool Exec(sqlite3 *db, const std::string &sql) {
  char *error = nullptr;
  if (sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &error) == SQLITE_OK)
    return true;

  std::println(stderr, "Error: {}", error ? error : "unknown error");
  sqlite3_free(error);
  return false;
}

[[nodiscard]] bool IsApplied(sqlite3 *db, const std::string &version) {
  sqlite3_stmt *stmt = nullptr;
  const char *sql = "SELECT 1 FROM schema_migrations WHERE version = ?";
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK)
    return false;

  sqlite3_bind_text(stmt, 1, version.c_str(), -1, SQLITE_STATIC);
  const bool applied = sqlite3_step(stmt) == SQLITE_ROW;
  sqlite3_finalize(stmt);
  return applied;
}

[[nodiscard]] bool RecordApplied(sqlite3 *db, const std::string &version) {
  sqlite3_stmt *stmt = nullptr;
  const char *sql = "INSERT INTO schema_migrations (version) VALUES (?)";
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    std::println(stderr, "Error preparing statement: {}", sqlite3_errmsg(db));
    return false;
  }

  sqlite3_bind_text(stmt, 1, version.c_str(), -1, SQLITE_STATIC);
  const bool inserted = sqlite3_step(stmt) == SQLITE_DONE;
  sqlite3_finalize(stmt);
  return inserted;
}

[[nodiscard]] bool ApplyMigration(sqlite3 *db, const Migration &migration) {
  const std::string sql = ReadFile(migration.path);

  const size_t up_pos = sql.find(kUpMarker);
  const size_t body_start =
      up_pos == std::string::npos ? std::string::npos : sql.find('\n', up_pos);
  if (body_start == std::string::npos) {
    std::println(stderr, "Error: {} has no '{}' marker",
                 migration.path.string(), kUpMarker);
    return false;
  }

  const size_t down_pos = sql.find(kDownMarker, up_pos);
  const std::string up_sql = sql.substr(
      body_start + 1, (down_pos == std::string::npos ? sql.size() : down_pos) -
                          (body_start + 1));

  if (!Exec(db, "BEGIN"))
    return false;

  if (!Exec(db, up_sql) || !RecordApplied(db, migration.version)) {
    (void)Exec(db, "ROLLBACK");
    return false;
  }

  return Exec(db, "COMMIT");
}

[[nodiscard]] std::vector<Migration>
DiscoverMigrations(const std::filesystem::path &dir) {
  std::vector<Migration> migrations;
  for (const auto &entry : std::filesystem::directory_iterator(dir)) {
    if (entry.path().extension() != ".sql")
      continue;

    const std::string filename = entry.path().filename().string();
    const size_t underscore = filename.find('_');
    migrations.push_back({.version = underscore == std::string::npos
                                         ? filename
                                         : filename.substr(0, underscore),
                          .path = entry.path()});
  }

  std::ranges::sort(migrations, {}, &Migration::path);
  return migrations;
}

[[nodiscard]] int RunMigrations(const std::string &database_path,
                                const std::filesystem::path &migrations_dir) {
  if (!std::filesystem::exists(migrations_dir)) {
    std::println(stderr, "Error: migrations directory '{}' does not exist",
                 migrations_dir.string());
    return 1;
  }

  sqlite3 *db = nullptr;
  if (sqlite3_open(database_path.c_str(), &db) != SQLITE_OK) {
    std::println(stderr, "Error opening database: {}", sqlite3_errmsg(db));
    sqlite3_close(db);
    return 1;
  }

  if (!Exec(
          db,
          R"(CREATE TABLE IF NOT EXISTS "schema_migrations" (version varchar(128) primary key))")) {
    sqlite3_close(db);
    return 1;
  }

  for (const auto &migration : DiscoverMigrations(migrations_dir)) {
    if (IsApplied(db, migration.version))
      continue;

    std::println("Applying: {}", migration.path.filename().string());
    if (!ApplyMigration(db, migration)) {
      sqlite3_close(db);
      return 1;
    }
  }

  sqlite3_close(db);
  return 0;
}

} // namespace

int main(int argc, char **argv) {
  char const *db_url_env = std::getenv("DB_URL");
  std::string const db_url = db_url_env ? db_url_env : "/app/data/ticketeer.db";

  char const *log_level_env = std::getenv("LOG_LEVEL");
  std::string const log_level = log_level_env ? log_level_env : "INFO";

  std::error_code ec;
  std::filesystem::create_directories(
      std::filesystem::path(db_url).parent_path(), ec);

  int const migrate_status = RunMigrations(db_url, "/app/migrations");
  if (migrate_status != 0)
    return migrate_status;

  std::vector<std::string> command;
  if (argc > 1) {
    for (int i = 1; i < argc; ++i)
      command.emplace_back(argv[i]);
  } else {
    command = {"ticketeer", "--bind",      "0.0.0.0", "--port",
               "5521",      "--log_level", log_level};
  }

  std::vector<char *> exec_argv;
  exec_argv.reserve(command.size() + 1);
  for (auto const &arg : command)
    exec_argv.push_back(const_cast<char *>(arg.c_str()));
  exec_argv.push_back(nullptr);

  std::fflush(stdout);
  execvp(exec_argv[0], exec_argv.data());
  std::cerr << "ticketeer-entrypoint: exec failed for " << exec_argv[0] << '\n';
  return 127;
}
