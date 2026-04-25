#include <exception>
#include <iostream>

#include "crow_all.h"
#include "ticketeer/apps/auth/routes.hpp"
#include "ticketeer/apps/helpdesk/departments/routes.hpp"
#include "ticketeer/apps/helpdesk/priorities/routes.hpp"
#include "ticketeer/apps/helpdesk/profiles/routes.hpp"
#include "ticketeer/apps/helpdesk/request-types/routes.hpp"
#include "ticketeer/apps/helpdesk/roles/administrator/routes.hpp"
#include "ticketeer/apps/helpdesk/roles/requester/routes.hpp"
#include "ticketeer/apps/helpdesk/roles/routes.hpp"
#include "ticketeer/apps/helpdesk/roles/supervisor/routes.hpp"
#include "ticketeer/apps/helpdesk/roles/technician/routes.hpp"
#include "ticketeer/apps/helpdesk/ticket-statuses/routes.hpp"
#include "ticketeer/apps/helpdesk/tickets/routes.hpp"
#include "ticketeer/core/crow_log.hpp"
#include "ticketeer/core/log.hpp"
#include "ticketeer/core/options.hpp"

static inline int RunTicketeerApi(int argc, char **argv) {
  const auto opts = ticketeer::core::Options::Parse(argc, argv);

  ticketeer::core::SetupLogging(opts.log_level, opts.log_color, opts.log_format);

  static ticketeer::core::CrowLogHandler crow_log_handler;
  crow::logger::setHandler(&crow_log_handler);

  crow::SimpleApp app;

  CROW_ROUTE(app, "/")([]() { return "😎"; });
  CROW_ROUTE(app, "/ticketeer/api")([]() { return "😎"; });

  ticketeer::apps::auth::RegisterRoutes(app);
  ticketeer::apps::helpdesk::roles::RegisterRoutes(app);
  ticketeer::apps::helpdesk::roles::administrator::RegisterRoutes(app);
  ticketeer::apps::helpdesk::roles::supervisor::RegisterRoutes(app);
  ticketeer::apps::helpdesk::roles::technician::RegisterRoutes(app);
  ticketeer::apps::helpdesk::roles::requester::RegisterRoutes(app);
  ticketeer::apps::helpdesk::departments::RegisterRoutes(app);
  ticketeer::apps::helpdesk::priorities::RegisterRoutes(app);
  ticketeer::apps::helpdesk::profiles::RegisterRoutes(app);
  ticketeer::apps::helpdesk::request_types::RegisterRoutes(app);
  ticketeer::apps::helpdesk::ticket_statuses::RegisterRoutes(app);
  ticketeer::apps::helpdesk::tickets::RegisterRoutes(app);

  auto &server = app.port(opts.port);
  if (opts.nothreaded)
    server.run();
  else if (opts.noconcurrency)
    server.concurrency(1).run();
  else
    server.multithreaded().run();

  return 0;
}

int main(int argc, char **argv) {
  try {
    return RunTicketeerApi(argc, argv);
  } catch (const ticketeer::core::OptionsParseError &ex) {
    std::cerr << ex.what() << '\n';
    return ex.exit_code;
  } catch (const std::exception &ex) {
    std::cerr << ex.what() << '\n';
    return 1;
  } catch (...) {
    std::cerr << "Unknown error\n";
    return 1;
  }
}
