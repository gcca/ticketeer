#include <drogon/drogon.h>
#include <trantor/utils/Logger.h>

#include "ticketeer/core/conf.hpp"
#include "ticketeer/core/logging.hpp"
#include "ticketeer/core/options.hpp"
#include "ticketeer/core/pulse.hpp"

void IndexHandler(const drogon::HttpRequestPtr &,
                  std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
  cb(drogon::HttpResponse::newRedirectionResponse("/ticketeer/auth/signin"));
}

int main(int argc, char *argv[]) {
  const auto options = ticketeer::core::options::InitServerOptions(argc, argv);
  const auto &settings = ticketeer::core::conf::settings;

  trantor::Logger::setLogLevel(
      ticketeer::core::logging::ParseLogLevel(options.log_level));

  LOG_INFO << "ticketeer: starting"
           << " DB_URL=" << settings.DB_URL
           << " UPLOAD_DIR=" << settings.UPLOAD_DIR << " TZ=" << settings.TZ
           << " bind=" << options.bind << " port=" << options.port
           << " log_level=" << options.log_level;

  ticketeer::core::pulse::Start();

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
