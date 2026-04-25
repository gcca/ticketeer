#include "internal.hpp"

#include "ticketeer/db/pg.hpp"

namespace ticketeer::apps::auth {

template crow::response SignIn<ticketeer::db::Pg>(const crow::request &,
                                                 ticketeer::db::Pg &,
                                                 ticketeer::core::Context &);

}
