#pragma once

#include <string>
#include <string_view>
#include <vector>

#include <quill/sinks/JsonSink.h>

namespace ticketeer::core::logging {

class TicketeerJsonConsoleSink : public quill::JsonConsoleSink {
  void AppendJsonString(std::string_view value);
  void AppendJsonField(std::string_view name, std::string_view value);
  void generate_json_message(
      quill::MacroMetadata const *log_metadata, std::uint64_t log_timestamp,
      std::string_view thread_id, std::string_view /* thread_name */,
      std::string const &process_id, std::string_view logger_name,
      quill::LogLevel log_level, std::string_view log_level_description,
      std::string_view log_level_short_code,
      std::vector<std::pair<std::string, std::string>> const *named_args,
      std::string_view log_message, std::string_view log_statement,
      char const *message_format) override;
};

[[nodiscard]] quill::LogLevel ParseLogLevel(const std::string &level);

} // namespace ticketeer::core::logging
