#pragma once

#include <string>

#include <crow_all.h>

#include "ticketeer/core/context.hpp"
#include "ticketeer/core/pagination.hpp"
#include "ticketeer/db/base.hpp"

namespace ticketeer::apps::helpdesk::departments {

template <ticketeer::db::Db DB>
[[nodiscard]] crow::response ListDepartments(const crow::request &req, DB &db,
                                             ticketeer::core::Context &) {
  const auto page = ticketeer::core::PageParams::FromRequest(req);
  const std::string limit_str = std::to_string(page.limit());
  const std::string offset_str = std::to_string(page.offset());
  const char *params[] = {limit_str.c_str(), offset_str.c_str()};
  const auto res = db.ExecParams(
      "SELECT id, name, description, created_at, COUNT(*) OVER() AS total"
      " FROM ticketeer.helpdesk_department"
      " ORDER BY name"
      " LIMIT $1 OFFSET $2",
      params, 2);
  if (!res.ok)
    return crow::response(503, res.error);

  crow::json::wvalue out;
  out["total"] = res.nrows() > 0 ? std::stoll(res.value(0, 4)) : 0LL;
  out["page"] = page.page;
  out["page_size"] = page.page_size;
  for (int i = 0; i < res.nrows(); ++i) {
    out["items"][i]["id"] = std::stoll(res.value(i, 0));
    out["items"][i]["name"] = res.value(i, 1);
    out["items"][i]["description"] = res.value(i, 2);
    out["items"][i]["created_at"] = res.value(i, 3);
  }
  return crow::response(200, out);
}

template <ticketeer::db::Db DB>
[[nodiscard]] crow::response GetDepartment(const crow::request &, DB &db,
                                           ticketeer::core::Context &, int id) {
  const auto id_str = std::to_string(id);
  const char *params[] = {id_str.c_str()};
  const auto res =
      db.ExecParams("SELECT id, name, description, created_at"
                    " FROM ticketeer.helpdesk_department WHERE id = $1",
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
  out["description"] = res.value(0, 2);
  out["created_at"] = res.value(0, 3);
  return crow::response(200, out);
}

template <ticketeer::db::Db DB>
[[nodiscard]] crow::response CreateDepartment(const crow::request &req, DB &db,
                                              ticketeer::core::Context &) {
  auto body = crow::json::load(req.body);
  if (!body || !body.has("name"))
    return crow::response(400, "Invalid request body");

  const std::string name = body["name"].s();
  const std::string description =
      body.has("description") ? std::string{body["description"].s()} : "";

  const char *params[] = {name.c_str(), description.c_str()};
  const auto res = db.ExecParams(
      "INSERT INTO ticketeer.helpdesk_department (name, description)"
      " VALUES ($1, $2) RETURNING id",
      params, 2);
  if (!res.ok)
    return crow::response(503, res.error);

  crow::json::wvalue out;
  out["id"] = std::stoll(res.value(0, 0));
  return crow::response(201, out);
}

template <ticketeer::db::Db DB>
[[nodiscard]] crow::response UpdateDepartment(const crow::request &req, DB &db,
                                              ticketeer::core::Context &,
                                              int id) {
  auto body = crow::json::load(req.body);
  if (!body || !body.has("name") || !body.has("description"))
    return crow::response(400, "Invalid request body");

  const std::string name = body["name"].s();
  const std::string description = body["description"].s();
  const std::string id_str = std::to_string(id);

  const char *params[] = {name.c_str(), description.c_str(), id_str.c_str()};
  const auto res =
      db.ExecParams("UPDATE ticketeer.helpdesk_department"
                    " SET name = $1, description = $2 WHERE id = $3",
                    params, 3);
  if (!res.ok)
    return crow::response(503, res.error);

  return crow::response(200);
}

template <ticketeer::db::Db DB>
[[nodiscard]] crow::response PatchDepartment(const crow::request &req, DB &db,
                                             ticketeer::core::Context &,
                                             int id) {
  auto body = crow::json::load(req.body);
  if (!body)
    return crow::response(400, "Invalid request body");

  std::string name_str, desc_str;
  const char *name_ptr = nullptr;
  const char *desc_ptr = nullptr;

  if (body.has("name")) {
    name_str = body["name"].s();
    name_ptr = name_str.c_str();
  }
  if (body.has("description")) {
    desc_str = body["description"].s();
    desc_ptr = desc_str.c_str();
  }

  const std::string id_str = std::to_string(id);
  const char *params[] = {name_ptr, desc_ptr, id_str.c_str()};
  const auto res = db.ExecParams("UPDATE ticketeer.helpdesk_department"
                                 " SET name = COALESCE($1, name),"
                                 "     description = COALESCE($2, description)"
                                 " WHERE id = $3",
                                 params, 3);
  if (!res.ok)
    return crow::response(503, res.error);

  return crow::response(200);
}

template <ticketeer::db::Db DB>
[[nodiscard]] crow::response DeleteDepartment(const crow::request &, DB &db,
                                              ticketeer::core::Context &,
                                              int id) {
  const std::string id_str = std::to_string(id);
  const char *params[] = {id_str.c_str()};
  const auto res = db.ExecParams(
      "DELETE FROM ticketeer.helpdesk_department WHERE id = $1", params, 1);
  if (!res.ok)
    return crow::response(503, res.error);

  return crow::response(204);
}

} // namespace ticketeer::apps::helpdesk::departments
