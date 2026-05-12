#pragma once

#include <map>
#include <string>
#include <vector>

#include <sqlite3.h>

namespace ticketeer::core::db {

using Row = std::map<std::string, std::string>;
using Rows = std::vector<Row>;

[[nodiscard]] sqlite3 *ConnectDB();

[[nodiscard]] inline std::string ColumnText(sqlite3_stmt *stmt, int i) {
  const auto *v = sqlite3_column_text(stmt, i);
  return v ? reinterpret_cast<const char *>(v) : "";
}

} // namespace ticketeer::core::db
