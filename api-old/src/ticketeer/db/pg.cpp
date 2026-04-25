#include "pg.hpp"

#include <libpq-fe.h>

#include "ticketeer/core/settings.hpp"

namespace ticketeer::db {

namespace {

inline PgHandle *H(pg_conn *p) { return reinterpret_cast<PgHandle *>(p); }
inline pg_conn *C(PgHandle *p) { return reinterpret_cast<pg_conn *>(p); }
inline PgResult *R(pg_result *p) { return reinterpret_cast<PgResult *>(p); }
inline pg_result *U(PgResult *p) { return reinterpret_cast<pg_result *>(p); }

} // namespace

Pg::Pg() : pg_{H(PQconnectdb(ticketeer::core::settings.db_conn.c_str()))} {}
Pg::~Pg() { PQfinish(C(pg_)); }

bool Pg::connected() const { return PQstatus(C(pg_)) == CONNECTION_OK; }
std::string Pg::conn_error() const { return PQerrorMessage(C(pg_)); }

Result Pg::Exec(const std::string &sql) {
  return FromResult(R(PQexec(C(pg_), sql.c_str())));
}

Result Pg::ExecParams(const std::string &sql, const char *const *params,
                      int nparams) {
  return FromResult(R(PQexecParams(C(pg_), sql.c_str(), nparams, nullptr,
                                   params, nullptr, nullptr, 0)));
}

Result Pg::FromResult(PgResult *res) {
  Result dr;
  ExecStatusType s = PQresultStatus(U(res));
  if (s != PGRES_TUPLES_OK && s != PGRES_COMMAND_OK) {
    dr.error = PQresultErrorMessage(U(res));
    PQclear(U(res));
    return dr;
  }
  dr.ok = true;
  dr.res_ = res;
  return dr;
}

} // namespace ticketeer::db
