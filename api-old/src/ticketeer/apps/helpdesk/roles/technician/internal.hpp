#pragma once

#include <string>

#include <crow_all.h>

#include "ticketeer/core/context.hpp"
#include "ticketeer/db/base.hpp"

namespace ticketeer::apps::helpdesk::roles::technician {

template <ticketeer::db::Db DB>
[[nodiscard]] crow::response ListTickets(const crow::request &, DB &db,
                                         ticketeer::core::Context &ctx) {
  const char *params[] = {ctx.token.data()};
  const auto profile = db.ExecParams(
      "SELECT hp.id FROM ticketeer.auth_session s"
      " JOIN ticketeer.helpdesk_profile hp ON hp.user_id = s.user_id"
      " WHERE s.token = $1 AND hp.role = 'technician' AND s.expires_at > now()",
      params, 1);
  if (!profile.ok)
    return crow::response(503, profile.error);
  if (profile.nrows() == 0) {
    crow::json::wvalue err;
    err["error"] = "forbidden";
    return crow::response(403, err);
  }

  const std::string profile_id = profile.value(0, 0);
  const char *tparams[] = {profile_id.c_str()};
  const auto res = db.ExecParams(
      "SELECT t.id, t.description,"
      "       p.display_name AS priority,"
      "       s.display_name AS status,"
      "       t.created_at,"
      "       ru.username AS created_by"
      " FROM ticketeer.helpdesk_ticket t"
      " JOIN ticketeer.helpdesk_priority p ON p.id = t.priority_id"
      " JOIN ticketeer.helpdesk_ticket_status s ON s.id = t.status_id"
      " JOIN ticketeer.helpdesk_profile rp ON rp.id = t.requester_id"
      " JOIN ticketeer.auth_user ru ON ru.id = rp.user_id"
      " WHERE t.assigned_to_id = $1::bigint"
      " ORDER BY t.created_at DESC"
      " LIMIT 50",
      tparams, 1);
  if (!res.ok)
    return crow::response(503, res.error);

  crow::json::wvalue out;
  for (int i = 0; i < res.nrows(); ++i) {
    out[i]["id"] = std::stoll(res.value(i, 0));
    out[i]["description"] = res.value(i, 1);
    out[i]["priority"] = res.value(i, 2);
    out[i]["status"] = res.value(i, 3);
    out[i]["created_at"] = res.value(i, 4);
    out[i]["created_by"] = res.value(i, 5);
  }
  return crow::response(200, out);
}

} // namespace ticketeer::apps::helpdesk::roles::technician
