#pragma once

#include <concepts>
#include <string>

namespace ticketeer::db {

template <class R>
concept DbResult = requires(const R &r, int i) {
  { r.ok } -> std::convertible_to<bool>;
  { r.error } -> std::convertible_to<std::string>;
  { r.nrows() } -> std::convertible_to<int>;
  { r.value(i, i) } -> std::convertible_to<const char *>;
};

template <class D>
concept Db =
    requires(D &db, const std::string &sql, const char *const *params, int n) {
      { db.connected() } -> std::convertible_to<bool>;
      { db.conn_error() } -> std::convertible_to<std::string>;
      { db.Exec(sql) } -> DbResult;
      { db.ExecParams(sql, params, n) } -> DbResult;
    };

} // namespace ticketeer::db
