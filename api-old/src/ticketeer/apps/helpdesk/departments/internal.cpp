#include "internal.hpp"

#include "ticketeer/db/pg.hpp"

namespace ticketeer::apps::helpdesk::departments {

template crow::response
ListDepartments<ticketeer::db::Pg>(const crow::request &, ticketeer::db::Pg &,
                                  ticketeer::core::Context &);

template crow::response
GetDepartment<ticketeer::db::Pg>(const crow::request &, ticketeer::db::Pg &,
                                ticketeer::core::Context &, int);

template crow::response
CreateDepartment<ticketeer::db::Pg>(const crow::request &, ticketeer::db::Pg &,
                                   ticketeer::core::Context &);

template crow::response
UpdateDepartment<ticketeer::db::Pg>(const crow::request &, ticketeer::db::Pg &,
                                   ticketeer::core::Context &, int);

template crow::response
PatchDepartment<ticketeer::db::Pg>(const crow::request &, ticketeer::db::Pg &,
                                  ticketeer::core::Context &, int);

template crow::response
DeleteDepartment<ticketeer::db::Pg>(const crow::request &, ticketeer::db::Pg &,
                                   ticketeer::core::Context &, int);

} // namespace ticketeer::apps::helpdesk::departments