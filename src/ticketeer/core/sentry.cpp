#include "sentry.hpp"

#include <cstddef>
#include <cstdint>

#include <sentry.h>
#include <trantor/utils/Logger.h>

namespace ticketeer::core::sentry {

bool Init(const conf::Settings &settings) {
  sentry_options_t *options = sentry_options_new();
  sentry_options_set_dsn(options, settings.SENTRY_DSN.c_str());
  sentry_options_set_release(options, "ticketeer");
  sentry_options_set_backend(options, nullptr);
  sentry_options_set_environment(options, "development");
#ifdef TICKETEER_SENTRY_DEBUG
  sentry_options_set_debug(options, 1);
#endif

  if (sentry_init(options)) {
    LOG_ERROR << "ticketeer: failed to initialize sentry";
    return false;
  }

  return true;
}

void StartupEvent(const options::Options &options,
                  const conf::Settings &settings) {
  sentry_value_t event = sentry_value_new_message_event(
      SENTRY_LEVEL_INFO, "ticketeer.lifecycle", "ticketeer: instance started");

  sentry_value_t metadata = sentry_value_new_object();
  sentry_value_set_by_key(metadata, "bind",
                          sentry_value_new_string(options.bind.c_str()));
  sentry_value_set_by_key(
      metadata, "port",
      sentry_value_new_int32(static_cast<std::int32_t>(options.port)));
  sentry_value_set_by_key(metadata, "log_level",
                          sentry_value_new_string(options.log_level.c_str()));
  sentry_value_set_by_key(metadata, "timezone",
                          sentry_value_new_string(settings.TZ.c_str()));
#ifdef TICKETEER_PULSE
  sentry_value_set_by_key(metadata, "pulse_enabled",
                          sentry_value_new_bool(true));
#else
  sentry_value_set_by_key(metadata, "pulse_enabled",
                          sentry_value_new_bool(false));
#endif

  sentry_value_t contexts = sentry_value_new_object();
  sentry_value_set_by_key(contexts, "ticketeer", metadata);
  sentry_value_set_by_key(event, "contexts", contexts);

  sentry_value_t tags = sentry_value_new_object();
  sentry_value_set_by_key(tags, "component", sentry_value_new_string("server"));
  sentry_value_set_by_key(tags, "lifecycle",
                          sentry_value_new_string("startup"));
  sentry_value_set_by_key(event, "tags", tags);

  const sentry_uuid_t event_id = sentry_capture_event(event);
  if (sentry_uuid_is_nil(&event_id)) {
    LOG_ERROR << "ticketeer: sentry startup event was not captured";
    return;
  }

  char event_uuid[37];
  sentry_uuid_as_string(&event_id, event_uuid);

  char event_id_string[33];
  std::size_t event_id_index = 0;
  for (const char character : event_uuid) {
    if (character == '\0') {
      break;
    }
    if (character != '-') {
      event_id_string[event_id_index++] = character;
    }
  }
  event_id_string[event_id_index] = '\0';

  LOG_INFO << "ticketeer: sentry startup event ID=" << event_id_string;
}

void Close() { sentry_close(); }

} // namespace ticketeer::core::sentry
