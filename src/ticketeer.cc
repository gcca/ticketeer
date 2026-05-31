#include <drogon/drogon.h>
#include <quill/Backend.h>
#include <quill/Frontend.h>
#include <quill/LogMacros.h>
#include <quill/Logger.h>

#include "ticketeer/core/conf.hpp"
#include "ticketeer/core/logging.hpp"
#include "ticketeer/core/options.hpp"

void IndexHandler(const drogon::HttpRequestPtr &,
                  std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
  cb(drogon::HttpResponse::newRedirectionResponse("/ticketeer/auth/signin"));
}

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

  LOGJ_INFO(logger, "ticketeer: starting", settings.DB_URL, settings.UPLOAD_DIR,
            options.bind, options.port, options.log_level);

  drogon::app()
      .registerHandler("/", &IndexHandler)
      .registerHandler("/ticketeer/", &IndexHandler)
      .registerHandler(
          "/ticketeer/healthcheck",
          [](const drogon::HttpRequestPtr &,
             std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
            auto response = drogon::HttpResponse::newHttpResponse();
            response->setBody("🍺");
            callback(response);
          })
      .setDocumentRoot("./static")
      .setUploadPath("")
      .setClientMaxBodySize(10L * 1024 * 1024)
      .setClientMaxMemoryBodySize(10L * 1024 * 1024)
      .addListener(options.bind, options.port)
      .run();
}
