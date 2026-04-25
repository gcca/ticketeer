#include "internal.hpp"

#include "ticketeer/db/pg.hpp"

namespace ticketeer::apps::helpdesk::ticket_statuses {

template crow::response
ListTicketStatuses<ticketeer::db::Pg>(const crow::request &, ticketeer::db::Pg &,
                                     ticketeer::core::Context &);

template crow::response
GetTicketStatus<ticketeer::db::Pg>(const crow::request &, ticketeer::db::Pg &,
                                  ticketeer::core::Context &, int);

template crow::response
CreateTicketStatus<ticketeer::db::Pg>(const crow::request &, ticketeer::db::Pg &,
                                     ticketeer::core::Context &);

template crow::response
UpdateTicketStatus<ticketeer::db::Pg>(const crow::request &, ticketeer::db::Pg &,
                                     ticketeer::core::Context &, int);

template crow::response
PatchTicketStatus<ticketeer::db::Pg>(const crow::request &, ticketeer::db::Pg &,
                                    ticketeer::core::Context &, int);

template crow::response
DeleteTicketStatus<ticketeer::db::Pg>(const crow::request &, ticketeer::db::Pg &,
                                     ticketeer::core::Context &, int);

} // namespace ticketeer::apps::helpdesk::ticket_statuses