#include "internal.hpp"

#include "ticketeer/db/pg.hpp"

namespace ticketeer::apps::helpdesk::roles::administrator {

template crow::response
ListTechnicians<ticketeer::db::Pg>(const crow::request &, ticketeer::db::Pg &,
                                  ticketeer::core::Context &);

template crow::response
AdminTickets<ticketeer::db::Pg>(const crow::request &, ticketeer::db::Pg &,
                               ticketeer::core::Context &);

} // namespace ticketeer::apps::helpdesk::roles::administrator