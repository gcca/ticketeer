#pragma once

#include <crow_all.h>

#include "context.hpp"
#include "ticketeer/db/base.hpp"
#include "ticketeer/db/pg.hpp"

namespace ticketeer::core {

namespace detail {

template <class... Ds> struct Chain;

template <> struct Chain<> {
  template <ticketeer::db::Db DB, class Fn>
  static crow::response Call(const crow::request &, DB &, Context &, Fn fn) {
    return fn();
  }
};

template <class D, class... Ds> struct Chain<D, Ds...> {
  template <ticketeer::db::Db DB, class Fn>
  static crow::response Call(const crow::request &req, DB &db, Context &ctx,
                             Fn fn) {
    return D::Call(req, db, ctx,
                   [&] { return Chain<Ds...>::Call(req, db, ctx, fn); });
  }
};

} // namespace detail

template <class... Ds, ticketeer::db::Db DB, class Fn>
[[nodiscard]] crow::response Required(const crow::request &req, DB &db,
                                      Context &ctx, Fn fn) {
  return detail::Chain<Ds...>::Call(req, db, ctx, fn);
}

template <class... Ds, class Fn> auto Handler(Fn fn) {
  return [fn](const crow::request &req) -> crow::response {
    ticketeer::db::Pg db;
    if (!db.connected())
      return crow::response(503, db.conn_error());
    ticketeer::core::Context ctx;
    return ticketeer::core::Required<Ds...>(req, db, ctx,
                                           [&] { return fn(req, db, ctx); });
  };
}

template <class... Ds, class Fn> auto HandlerId(Fn fn) {
  return [fn](const crow::request &req, int id) -> crow::response {
    ticketeer::db::Pg db;
    if (!db.connected())
      return crow::response(503, db.conn_error());
    ticketeer::core::Context ctx;
    return ticketeer::core::Required<Ds...>(
        req, db, ctx, [&] { return fn(req, db, ctx, id); });
  };
}

} // namespace ticketeer::core
