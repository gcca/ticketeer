#include "internal.hpp"

#include "ticketeer/db/pg.hpp"

namespace ticketeer::apps::helpdesk::roles::requester {

template crow::response
RequestTicket<ticketeer::db::Pg>(const crow::request &, ticketeer::db::Pg &,
                                ticketeer::core::Context &);

template crow::response
ListRequestedBy<ticketeer::db::Pg>(const crow::request &, ticketeer::db::Pg &,
                                  ticketeer::core::Context &, int);

} // namespace ticketeer::apps::helpdesk::roles::requester