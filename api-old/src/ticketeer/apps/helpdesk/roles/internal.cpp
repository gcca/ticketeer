#include "internal.hpp"

#include "ticketeer/db/pg.hpp"

namespace ticketeer::apps::helpdesk::roles {

template crow::response
RolesContent<ticketeer::db::Pg>(const crow::request &, ticketeer::db::Pg &,
                               ticketeer::core::Context &);

}