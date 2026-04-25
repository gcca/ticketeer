#include "result.hpp"

#include <libpq-fe.h>

namespace ticketeer::db {

namespace {

inline pg_result *U(PgResult *p) { return reinterpret_cast<pg_result *>(p); }

} // namespace

Result::~Result() {
  if (res_)
    PQclear(U(res_));
}

Result::Result(Result &&o) noexcept
    : ok{o.ok}, error{std::move(o.error)}, res_{o.res_} {
  o.res_ = nullptr;
}

Result &Result::operator=(Result &&o) noexcept {
  if (this != &o) {
    if (res_)
      PQclear(U(res_));
    ok = o.ok;
    error = std::move(o.error);
    res_ = o.res_;
    o.res_ = nullptr;
  }
  return *this;
}

int Result::nrows() const { return PQntuples(U(res_)); }
int Result::ncols() const { return PQnfields(U(res_)); }
const char *Result::value(int r, int c) const {
  return PQgetvalue(U(res_), r, c);
}

} // namespace ticketeer::db
