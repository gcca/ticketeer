#pragma once

#include <string>

#include "result.hpp"

namespace ticketeer::db {

struct PgHandle;
struct PgResult;

class Pg {
public:
  Pg();
  ~Pg();

  [[nodiscard]] bool connected() const;
  [[nodiscard]] std::string conn_error() const;
  [[nodiscard]] Result Exec(const std::string &sql);
  [[nodiscard]] Result ExecParams(const std::string &sql,
                                  const char *const *params, int nparams);

private:
  PgHandle *pg_;

  [[nodiscard]] static Result FromResult(PgResult *res);
};

} // namespace ticketeer::db
