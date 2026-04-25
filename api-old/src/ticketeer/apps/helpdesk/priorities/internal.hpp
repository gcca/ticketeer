#pragma once

#include <string>

#include <crow_all.h>

#include "ticketeer/core/context.hpp"
#include "ticketeer/core/pagination.hpp"
#include "ticketeer/db/base.hpp"

namespace ticketeer::apps::helpdesk::priorities {

template <ticketeer::db::Db DB>
[[nodiscard]] crow::response ListPriorities(const crow::request &req, DB &db,
                                            ticketeer::core::Context &) {
  const auto page = ticketeer::core::PageParams::FromRequest(req);
  const std::string limit_str = std::to_string(page.limit());
  const std::string offset_str = std::to_string(page.offset());
  const char *params[] = {limit_str.c_str(), offset_str.c_str()};
  const auto res =
      db.ExecParams("SELECT id, name, display_name, COUNT(*) OVER() AS total"
                    " FROM ticketeer.helpdesk_priority"
                    " ORDER BY id"
                    " LIMIT $1 OFFSET $2",
                    params, 2);
  if (!res.ok)
    return crow::response(503, res.error);

  crow::json::wvalue out;
  out["total"] = res.nrows() > 0 ? std::stoll(res.value(0, 3)) : 0LL;
  out["page"] = page.page;
  out["page_size"] = page.page_size;
  for (int i = 0; i < res.nrows(); ++i) {
    out["items"][i]["id"] = std::stoll(res.value(i, 0));
    out["items"][i]["name"] = res.value(i, 1);
    out["items"][i]["display_name"] = res.value(i, 2);
  }
  return crow::response(200, out);
}

template <ticketeer::db::Db DB>
[[nodiscard]] crow::response GetPriority(const crow::request &, DB &db,
                                         ticketeer::core::Context &, int id) {
  const std::string id_str = std::to_string(id);
  const char *params[] = {id_str.c_str()};
  const auto res =
      db.ExecParams("SELECT id, name, display_name"
                    " FROM ticketeer.helpdesk_priority WHERE id = $1",
                    params, 1);
  if (!res.ok)
    return crow::response(503, res.error);
  if (res.nrows() == 0) {
    crow::json::wvalue err;
    err["error"] = "not_found";
    return crow::response(404, err);
  }

  crow::json::wvalue out;
  out["id"] = std::stoll(res.value(0, 0));
  out["name"] = res.value(0, 1);
  out["display_name"] = res.value(0, 2);
  return crow::response(200, out);
}

template <ticketeer::db::Db DB>
[[nodiscard]] crow::response CreatePriority(const crow::request &req, DB &db,
                                            ticketeer::core::Context &) {
  auto body = crow::json::load(req.body);
  if (!body || !body.has("name") || !body.has("display_name"))
    return crow::response(400, "Invalid request body");

  const std::string name = body["name"].s();
  const std::string display_name = body["display_name"].s();

  const char *params[] = {name.c_str(), display_name.c_str()};
  const auto res = db.ExecParams(
      "INSERT INTO ticketeer.helpdesk_priority (name, display_name)"
      " VALUES ($1, $2) RETURNING id",
      params, 2);
  if (!res.ok)
    return crow::response(503, res.error);

  crow::json::wvalue out;
  out["id"] = std::stoll(res.value(0, 0));
  return crow::response(201, out);
}

template <ticketeer::db::Db DB>
[[nodiscard]] crow::response UpdatePriority(const crow::request &req, DB &db,
                                            ticketeer::core::Context &, int id) {
  auto body = crow::json::load(req.body);
  if (!body || !body.has("name") || !body.has("display_name"))
    return crow::response(400, "Invalid request body");

  const std::string name = body["name"].s();
  const std::string display_name = body["display_name"].s();
  const std::string id_str = std::to_string(id);

  const char *params[] = {name.c_str(), display_name.c_str(), id_str.c_str()};
  const auto res =
      db.ExecParams("UPDATE ticketeer.helpdesk_priority"
                    " SET name = $1, display_name = $2 WHERE id = $3",
                    params, 3);
  if (!res.ok)
    return crow::response(503, res.error);

  return crow::response(200);
}

template <ticketeer::db::Db DB>
[[nodiscard]] crow::response PatchPriority(const crow::request &req, DB &db,
                                           ticketeer::core::Context &, int id) {
  auto body = crow::json::load(req.body);
  if (!body)
    return crow::response(400, "Invalid request body");

  std::string name_str, display_name_str;
  const char *name_ptr = nullptr;
  const char *display_name_ptr = nullptr;

  if (body.has("name")) {
    name_str = body["name"].s();
    name_ptr = name_str.c_str();
  }
  if (body.has("display_name")) {
    display_name_str = body["display_name"].s();
    display_name_ptr = display_name_str.c_str();
  }

  const std::string id_str = std::to_string(id);
  const char *params[] = {name_ptr, display_name_ptr, id_str.c_str()};
  const auto res =
      db.ExecParams("UPDATE ticketeer.helpdesk_priority"
                    " SET name         = COALESCE($1, name),"
                    "     display_name = COALESCE($2, display_name)"
                    " WHERE id = $3",
                    params, 3);
  if (!res.ok)
    return crow::response(503, res.error);

  return crow::response(200);
}

template <ticketeer::db::Db DB>
[[nodiscard]] crow::response DeletePriority(const crow::request &, DB &db,
                                            ticketeer::core::Context &, int id) {
  const std::string id_str = std::to_string(id);
  const char *params[] = {id_str.c_str()};
  const auto res = db.ExecParams(
      "DELETE FROM ticketeer.helpdesk_priority WHERE id = $1", params, 1);
  if (!res.ok)
    return crow::response(503, res.error);

  return crow::response(204);
}

} // namespace ticketeer::apps::helpdesk::priorities
