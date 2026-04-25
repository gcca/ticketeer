#include "internal.hpp"

#include "ticketeer/db/pg.hpp"

namespace ticketeer::apps::helpdesk::priorities {

template crow::response
ListPriorities<ticketeer::db::Pg>(const crow::request &, ticketeer::db::Pg &,
                                 ticketeer::core::Context &);

template crow::response GetPriority<ticketeer::db::Pg>(const crow::request &,
                                                      ticketeer::db::Pg &,
                                                      ticketeer::core::Context &,
                                                      int);

template crow::response
CreatePriority<ticketeer::db::Pg>(const crow::request &, ticketeer::db::Pg &,
                                 ticketeer::core::Context &);

template crow::response
UpdatePriority<ticketeer::db::Pg>(const crow::request &, ticketeer::db::Pg &,
                                 ticketeer::core::Context &, int);

template crow::response
PatchPriority<ticketeer::db::Pg>(const crow::request &, ticketeer::db::Pg &,
                                ticketeer::core::Context &, int);

template crow::response
DeletePriority<ticketeer::db::Pg>(const crow::request &, ticketeer::db::Pg &,
                                 ticketeer::core::Context &, int);

} // namespace ticketeer::apps::helpdesk::priorities