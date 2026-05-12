#include <cstdlib>

#include <drogon/drogon.h>
#include <trantor/utils/Logger.h>

#include "ticketeer/core/conf.hpp"
#include "ticketeer/core/logging.hpp"
#include "ticketeer/core/options.hpp"
#ifdef TICKETEER_SENTRY
#include "ticketeer/core/sentry.hpp"
#endif
#ifdef TICKETEER_PULSE
#include "pulse.hpp"
#endif

int main(int argc, char *argv[]) {
  const auto options = ticketeer::core::options::Init(argc, argv);
  const auto &settings = ticketeer::core::conf::settings;

  trantor::Logger::setLogLevel(
      ticketeer::core::logging::ParseLogLevel(options.log_level));

#ifdef TICKETEER_SENTRY
  if (!ticketeer::core::sentry::Init(settings)) {
    return EXIT_FAILURE;
  }
  ticketeer::core::sentry::StartupEvent(options, settings);
#endif

  LOG_INFO << "ticketeer: starting"
           << " db_path=" << settings.DB_PATH
           << " upload_dir=" << settings.UPLOAD_DIR << " tz=" << settings.TZ
           << " bind=" << options.bind << " port=" << options.port
           << " log_level=" << options.log_level;

#ifdef TICKETEER_PULSE
  ticketeer::core::pulse::Start();
#endif

  drogon::app()
      .setDocumentRoot("./static")
      .setUploadPath("")
      .setClientMaxBodySize(10L * 1024 * 1024)
      .setClientMaxMemoryBodySize(10L * 1024 * 1024)
      .addListener(options.bind, options.port)
      .run();

#ifdef TICKETEER_SENTRY
  ticketeer::core::sentry::Close();
#endif
}
