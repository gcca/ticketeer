#pragma once

#include <string_view>

#include <crow_all.h>

#include "ticketeer/core/context.hpp"
#include "ticketeer/db/base.hpp"

namespace ticketeer::apps::helpdesk::roles::administrator {

template <ticketeer::db::Db DB>
[[nodiscard]] crow::response ListTechnicians(const crow::request &, DB &db,
                                             ticketeer::core::Context &) {
  const auto res =
      db.Exec("SELECT hp.id, hp.user_id, hp.department_id, au.username"
              " FROM ticketeer.helpdesk_profile hp"
              " JOIN ticketeer.auth_user au ON au.id = hp.user_id"
              " WHERE hp.role = 'technician'"
              " ORDER BY au.username");
  if (!res.ok)
    return crow::response(503, res.error);

  crow::json::wvalue out;
  for (int i = 0; i < res.nrows(); ++i) {
    out[i]["id"] = std::stoll(res.value(i, 0));
    out[i]["user_id"] = std::stoll(res.value(i, 1));
    out[i]["department_id"] = std::stoll(res.value(i, 2));
    out[i]["username"] = res.value(i, 3);
  }
  return crow::response(200, out);
}

template <ticketeer::db::Db DB>
[[nodiscard]] crow::response AdminTickets(const crow::request &, DB &db,
                                          ticketeer::core::Context &) {
  const auto res = db.Exec(
      "SELECT t.id, t.description,"
      "       p.display_name AS priority,"
      "       s.display_name AS status,"
      "       t.created_at,"
      "       au.username    AS assigned_to,"
      "       ru.username    AS created_by"
      " FROM ticketeer.helpdesk_ticket t"
      " JOIN ticketeer.helpdesk_priority p ON p.id = t.priority_id"
      " JOIN ticketeer.helpdesk_ticket_status s ON s.id = t.status_id"
      " JOIN ticketeer.helpdesk_profile rp ON rp.id = t.requester_id"
      " JOIN ticketeer.auth_user ru ON ru.id = rp.user_id"
      " LEFT JOIN ticketeer.helpdesk_profile ap ON ap.id = t.assigned_to_id"
      " LEFT JOIN ticketeer.auth_user au ON au.id = ap.user_id"
      " ORDER BY t.created_at DESC"
      " LIMIT 100");
  if (!res.ok)
    return crow::response(503, res.error);

  crow::json::wvalue out;
  for (int i = 0; i < res.nrows(); ++i) {
    out[i]["id"] = std::stoll(res.value(i, 0));
    out[i]["description"] = res.value(i, 1);
    out[i]["priority"] = res.value(i, 2);
    out[i]["status"] = res.value(i, 3);
    out[i]["created_at"] = res.value(i, 4);
    out[i]["assigned_to"] = std::string_view{res.value(i, 5)}.empty()
                                ? crow::json::wvalue{}
                                : crow::json::wvalue{res.value(i, 5)};
    out[i]["created_by"] = res.value(i, 6);
  }
  return crow::response(200, out);
}

} // namespace ticketeer::apps::helpdesk::roles::administrator
