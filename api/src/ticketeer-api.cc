#include <drogon/drogon.h>
#include <quill/Backend.h>
#include <quill/Frontend.h>
#include <quill/LogMacros.h>
#include <quill/Logger.h>

#include "ticketeer/core/conf.hpp"
#include "ticketeer/core/logging.hpp"
#include "ticketeer/core/options.hpp"

int main(int argc, char *argv[]) {
  const auto options = ticketeer::core::options::InitServerOptions(argc, argv);
  const auto &settings = ticketeer::core::conf::settings;

  quill::Backend::start();
  auto sink = quill::Frontend::create_or_get_sink<
      ticketeer::core::logging::TicketeerJsonConsoleSink>("console");
  quill::Logger *logger = quill::Frontend::create_or_get_logger(
      "root", std::move(sink),
      quill::PatternFormatterOptions{"", "%H:%M:%S.%Qns",
                                     quill::Timezone::GmtTime});
  logger->set_log_level(
      ticketeer::core::logging::ParseLogLevel(options.log_level));

  LOG_INFO(logger, "ticketeer-api: starting");
  const bool db_url_configured = !settings.DB_URL.empty();
  LOGJ_INFO(logger, "ticketeer-api: settings loaded", db_url_configured);

  drogon::app().registerHandler(
      "/ticketeer/api",
      [](const drogon::HttpRequestPtr &,
         std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
        auto response = drogon::HttpResponse::newHttpResponse();
        response->setBody("😎");
        callback(response);
      });

  const auto &bind = options.bind;
  const auto port = options.port;
  LOGJ_INFO(logger, "ticketeer-api: listening", bind, port);

  drogon::app().addListener(options.bind, options.port).run();
}
