#pragma once

#include <string>

#include <crow_all.h>

#include "ticketeer/core/context.hpp"
#include "ticketeer/core/pagination.hpp"
#include "ticketeer/db/base.hpp"

namespace ticketeer::apps::helpdesk::ticket_statuses {

template <ticketeer::db::Db DB>
[[nodiscard]] crow::response ListTicketStatuses(const crow::request &req,
                                                DB &db,
                                                ticketeer::core::Context &) {
  const auto page = ticketeer::core::PageParams::FromRequest(req);
  const std::string limit_str = std::to_string(page.limit());
  const std::string offset_str = std::to_string(page.offset());
  const char *params[] = {limit_str.c_str(), offset_str.c_str()};
  const auto res = db.ExecParams(
      "SELECT id, name, display_name, trait, COUNT(*) OVER() AS total"
      " FROM ticketeer.helpdesk_ticket_status"
      " ORDER BY id"
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
    out["items"][i]["display_name"] = res.value(i, 2);
    out["items"][i]["trait"] = res.value(i, 3);
  }
  return crow::response(200, out);
}

template <ticketeer::db::Db DB>
[[nodiscard]] crow::response GetTicketStatus(const crow::request &, DB &db,
                                             ticketeer::core::Context &,
                                             int id) {
  const std::string id_str = std::to_string(id);
  const char *params[] = {id_str.c_str()};
  const auto res =
      db.ExecParams("SELECT id, name, display_name, trait"
                    " FROM ticketeer.helpdesk_ticket_status WHERE id = $1",
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
  out["trait"] = res.value(0, 3);
  return crow::response(200, out);
}

template <ticketeer::db::Db DB>
[[nodiscard]] crow::response CreateTicketStatus(const crow::request &req,
                                                DB &db,
                                                ticketeer::core::Context &) {
  auto body = crow::json::load(req.body);
  if (!body || !body.has("name") || !body.has("display_name") ||
      !body.has("trait"))
    return crow::response(400, "Invalid request body");

  const std::string name = body["name"].s();
  const std::string display_name = body["display_name"].s();
  const std::string trait = body["trait"].s();

  const char *params[] = {name.c_str(), display_name.c_str(), trait.c_str()};
  const auto res = db.ExecParams(
      "INSERT INTO ticketeer.helpdesk_ticket_status (name, display_name, trait)"
      " VALUES ($1, $2, $3::ticketeer.helpdesk_ticket_trait) RETURNING id",
      params, 3);
  if (!res.ok)
    return crow::response(503, res.error);

  crow::json::wvalue out;
  out["id"] = std::stoll(res.value(0, 0));
  return crow::response(201, out);
}

template <ticketeer::db::Db DB>
[[nodiscard]] crow::response
UpdateTicketStatus(const crow::request &req, DB &db, ticketeer::core::Context &,
                   int id) {
  auto body = crow::json::load(req.body);
  if (!body || !body.has("name") || !body.has("display_name") ||
      !body.has("trait"))
    return crow::response(400, "Invalid request body");

  const std::string name = body["name"].s();
  const std::string display_name = body["display_name"].s();
  const std::string trait = body["trait"].s();
  const std::string id_str = std::to_string(id);

  const char *params[] = {name.c_str(), display_name.c_str(), trait.c_str(),
                          id_str.c_str()};
  const auto res =
      db.ExecParams("UPDATE ticketeer.helpdesk_ticket_status"
                    " SET name = $1, display_name = $2,"
                    "     trait = $3::ticketeer.helpdesk_ticket_trait"
                    " WHERE id = $4",
                    params, 4);
  if (!res.ok)
    return crow::response(503, res.error);

  return crow::response(200);
}

template <ticketeer::db::Db DB>
[[nodiscard]] crow::response PatchTicketStatus(const crow::request &req, DB &db,
                                               ticketeer::core::Context &,
                                               int id) {
  auto body = crow::json::load(req.body);
  if (!body)
    return crow::response(400, "Invalid request body");

  std::string name_str, display_name_str, trait_str;
  const char *name_ptr = nullptr;
  const char *display_name_ptr = nullptr;
  const char *trait_ptr = nullptr;

  if (body.has("name")) {
    name_str = body["name"].s();
    name_ptr = name_str.c_str();
  }
  if (body.has("display_name")) {
    display_name_str = body["display_name"].s();
    display_name_ptr = display_name_str.c_str();
  }
  if (body.has("trait")) {
    trait_str = body["trait"].s();
    trait_ptr = trait_str.c_str();
  }

  const std::string id_str = std::to_string(id);
  const char *params[] = {name_ptr, display_name_ptr, trait_ptr,
                          id_str.c_str()};
  const auto res = db.ExecParams(
      "UPDATE ticketeer.helpdesk_ticket_status"
      " SET name         = COALESCE($1, name),"
      "     display_name = COALESCE($2, display_name),"
      "     trait        = COALESCE($3::ticketeer.helpdesk_ticket_trait, trait)"
      " WHERE id = $4",
      params, 4);
  if (!res.ok)
    return crow::response(503, res.error);

  return crow::response(200);
}

template <ticketeer::db::Db DB>
[[nodiscard]] crow::response DeleteTicketStatus(const crow::request &, DB &db,
                                                ticketeer::core::Context &,
                                                int id) {
  const std::string id_str = std::to_string(id);
  const char *params[] = {id_str.c_str()};
  const auto res = db.ExecParams(
      "DELETE FROM ticketeer.helpdesk_ticket_status WHERE id = $1", params, 1);
  if (!res.ok)
    return crow::response(503, res.error);

  return crow::response(204);
}

} // namespace ticketeer::apps::helpdesk::ticket_statuses
