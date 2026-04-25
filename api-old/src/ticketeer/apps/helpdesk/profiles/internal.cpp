#include "internal.hpp"

#include "ticketeer/db/pg.hpp"

namespace ticketeer::apps::helpdesk::profiles {

template crow::response
GetMyProfile<ticketeer::db::Pg>(const crow::request &, ticketeer::db::Pg &,
                               ticketeer::core::Context &);

}