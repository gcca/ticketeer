#include "internal.hpp"

#include "ticketeer/db/pg.hpp"

namespace ticketeer::apps::helpdesk::roles::supervisor {

template crow::response
ListTechnicians<ticketeer::db::Pg>(const crow::request &, ticketeer::db::Pg &,
                                  ticketeer::core::Context &);

template crow::response
ListTickets<ticketeer::db::Pg>(const crow::request &, ticketeer::db::Pg &,
                              ticketeer::core::Context &);

template crow::response GetTicket<ticketeer::db::Pg>(const crow::request &,
                                                    ticketeer::db::Pg &,
                                                    ticketeer::core::Context &,
                                                    int);

template crow::response
UpdateTicket<ticketeer::db::Pg>(const crow::request &, ticketeer::db::Pg &,
                               ticketeer::core::Context &, int);

} // namespace ticketeer::apps::helpdesk::roles::supervisor