#include "internal.hpp"

#include "ticketeer/db/pg.hpp"

namespace ticketeer::apps::helpdesk::request_types {

template crow::response
ListRequestTypes<ticketeer::db::Pg>(const crow::request &, ticketeer::db::Pg &,
                                   ticketeer::core::Context &);

template crow::response
GetRequestType<ticketeer::db::Pg>(const crow::request &, ticketeer::db::Pg &,
                                 ticketeer::core::Context &, int);

template crow::response
CreateRequestType<ticketeer::db::Pg>(const crow::request &, ticketeer::db::Pg &,
                                    ticketeer::core::Context &);

template crow::response
UpdateRequestType<ticketeer::db::Pg>(const crow::request &, ticketeer::db::Pg &,
                                    ticketeer::core::Context &, int);

template crow::response
PatchRequestType<ticketeer::db::Pg>(const crow::request &, ticketeer::db::Pg &,
                                   ticketeer::core::Context &, int);

template crow::response
DeleteRequestType<ticketeer::db::Pg>(const crow::request &, ticketeer::db::Pg &,
                                    ticketeer::core::Context &, int);

} // namespace ticketeer::apps::helpdesk::request_types