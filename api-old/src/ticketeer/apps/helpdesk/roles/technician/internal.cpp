#include "internal.hpp"

#include "ticketeer/db/pg.hpp"

namespace ticketeer::apps::helpdesk::roles::technician {

template crow::response
ListTickets<ticketeer::db::Pg>(const crow::request &, ticketeer::db::Pg &,
                              ticketeer::core::Context &);

}