#pragma once

#include <string_view>

#include <crow_all.h>

#include "ticketeer/core/context.hpp"
#include "ticketeer/db/base.hpp"

namespace ticketeer::apps::auth::decorators {

[[nodiscard]] static inline std::string_view
ExtractBearerToken(const crow::request &req) {
  const std::string &header = req.get_header_value("Authorization");
  constexpr std::string_view prefix = "Bearer ";
  if (header.size() <= prefix.size() ||
      std::string_view{header}.substr(0, prefix.size()) != prefix)
    return {};
  return std::string_view{header}.substr(prefix.size());
}

struct LoginRequired {
  template <ticketeer::db::Db DB, class Fn>
  static crow::response Call(const crow::request &req, DB &db,
                             ticketeer::core::Context &ctx, Fn next) {
    ctx.token = ExtractBearerToken(req);
    if (ctx.token.empty()) {
      crow::json::wvalue err;
      err["error"] = "unauthorized";
      return crow::response(401, err);
    }

    const char *params[] = {ctx.token.data()};
    const auto res = db.ExecParams("SELECT 1 FROM ticketeer.auth_session"
                                   " WHERE token = $1 AND expires_at > now()",
                                   params, 1);

    if (!res.ok)
      return crow::response(503, res.error);
    if (res.nrows() == 0) {
      crow::json::wvalue err;
      err["error"] = "unauthorized";
      return crow::response(401, err);
    }

    return next();
  }
};

} // namespace ticketeer::apps::auth::decorators
