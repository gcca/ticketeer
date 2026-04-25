#pragma once

#include <string>

namespace ticketeer::db {

struct PgResult;

struct Result {
  bool ok{false};
  std::string error;

  [[nodiscard]] int nrows() const;
  [[nodiscard]] int ncols() const;
  [[nodiscard]] const char *value(int r, int c) const;

  ~Result();
  Result() = default;
  Result(Result &&) noexcept;
  Result &operator=(Result &&) noexcept;
  Result(const Result &) = delete;
  Result &operator=(const Result &) = delete;

private:
  PgResult *res_{nullptr};

  friend class Pg;
};

} // namespace ticketeer::db
