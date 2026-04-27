#include "common.hpp"

#include <vector>

#include <quill/LogMacros.h>

namespace ticketeer::api::role::common {
namespace {

[[nodiscard]] Json::Value ReadTicketRows(PGresult *tickets) {
  Json::Value result = Json::arrayValue;
  for (int i = 0; i < PQntuples(tickets); ++i) {
    Json::Value ticket;
    ticket["id"] = PQgetvalue(tickets, i, 0);
    ticket["description"] = PQgetvalue(tickets, i, 1);
    ticket["status_id"] = PQgetvalue(tickets, i, 2);
    ticket["created_at"] = PQgetvalue(tickets, i, 3);
    ticket["activities"] = Json::arrayValue;
    result.append(ticket);
  }
  return result;
}

void AppendActivityPreviews(quill::Logger *logger, PGconn *pg,
                            const char *log_prefix, Json::Value &tickets) {
  for (Json::ArrayIndex i = 0; i < tickets.size(); ++i) {
    std::string ticket_id = tickets[i]["id"].asString();
    const char *params[] = {ticket_id.c_str()};
    PGresult *activities =
        PQexecParams(pg,
                     R"SQL(
SELECT body,
       created_at::text
  FROM helpdesk.ticket_activity
 WHERE ticket_id = $1::bigint
 ORDER BY created_at DESC
)SQL",
                     1, nullptr, params, nullptr, nullptr, 0);

    if (PQresultStatus(activities) != PGRES_TUPLES_OK) {
      LOGJ_DEBUG(logger, "{} ticket activity preview query error", log_prefix,
                 PQresultErrorMessage(activities));
      PQclear(activities);
      continue;
    }

    for (int j = 0; j < PQntuples(activities); ++j) {
      Json::Value activity;
      activity["body"] = PQgetvalue(activities, j, 0);
      activity["created_at"] = PQgetvalue(activities, j, 1);
      tickets[i]["activities"].append(activity);
    }
    PQclear(activities);
  }
}

[[nodiscard]] Json::Value
FetchTicketList(quill::Logger *logger, PGconn *pg, const char *log_prefix,
                const std::string *profile_id,
                const std::optional<std::string> &search) {
  std::vector<std::string> owned_params;
  std::vector<const char *> params;
  std::string query =
      "SELECT t.id::text, t.description, t.status_id::text, t.created_at::text "
      "FROM helpdesk.ticket t";

  std::vector<std::string> filters;
  if (profile_id) {
    params.push_back(profile_id->c_str());
    filters.push_back("t.requester_id = $" + std::to_string(params.size()) +
                      "::bigint");
  }

  if (search && !search->empty()) {
    owned_params.push_back("%" + *search + "%");
    params.push_back(owned_params.back().c_str());
    const std::string placeholder = "$" + std::to_string(params.size());
    filters.push_back("(t.description ILIKE " + placeholder +
                      " OR EXISTS (SELECT 1 FROM helpdesk.ticket_activity ta "
                      "WHERE ta.ticket_id = t.id AND ta.body ILIKE " +
                      placeholder + "))");
  }

  if (!filters.empty()) {
    query += " WHERE ";
    for (std::size_t i = 0; i < filters.size(); ++i) {
      if (i > 0) {
        query += " AND ";
      }
      query += filters[i];
    }
  }

  query +=
      " ORDER BY t.created_at DESC LIMIT " + std::to_string(TicketListLimit);

  PGresult *tickets = PQexecParams(
      pg, query.c_str(), static_cast<int>(params.size()), nullptr,
      params.empty() ? nullptr : params.data(), nullptr, nullptr, 0);

  Json::Value result = Json::arrayValue;
  if (PQresultStatus(tickets) != PGRES_TUPLES_OK) {
    LOGJ_DEBUG(logger, "{} ticket list query error", log_prefix,
               PQresultErrorMessage(tickets));
    PQclear(tickets);
    return result;
  }

  result = ReadTicketRows(tickets);
  PQclear(tickets);
  AppendActivityPreviews(logger, pg, log_prefix, result);
  return result;
}

} // namespace

Json::Value FetchRequesterTicketList(quill::Logger *logger, PGconn *pg,
                                     const std::string &profile_id,
                                     const std::optional<std::string> &search) {
  return FetchTicketList(logger, pg, "[requester]", &profile_id, search);
}

Json::Value
FetchSupervisorTicketList(quill::Logger *logger, PGconn *pg,
                          const std::optional<std::string> &search) {
  return FetchTicketList(logger, pg, "[supervisor]", nullptr, search);
}

} // namespace ticketeer::api::role::common
