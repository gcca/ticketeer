#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include <quill/sinks/JsonSink.h>

namespace ticketeer::core::logging {

class TicketeerJsonConsoleSink : public quill::JsonConsoleSink {
  void AppendJsonString(std::string_view value) {
    constexpr char hex[] = "0123456789abcdef";

    _json_message.push_back('"');
    for (const unsigned char character : value) {
      switch (character) {
      case '"':
        _json_message.append(std::string_view{R"(\")"});
        break;
      case '\\':
        _json_message.append(std::string_view{R"(\\)"});
        break;
      case '\b':
        _json_message.append(std::string_view{R"(\b)"});
        break;
      case '\f':
        _json_message.append(std::string_view{R"(\f)"});
        break;
      case '\n':
        _json_message.append(std::string_view{R"(\n)"});
        break;
      case '\r':
        _json_message.append(std::string_view{R"(\r)"});
        break;
      case '\t':
        _json_message.append(std::string_view{R"(\t)"});
        break;
      default:
        if (character < 0x20) {
          const char escaped[] = {
              '\\', 'u', '0', '0', hex[character >> 4], hex[character & 0x0f]};
          _json_message.append(
              std::string_view{escaped, sizeof(escaped) / sizeof(escaped[0])});
        } else {
          _json_message.push_back(static_cast<char>(character));
        }
      }
    }
    _json_message.push_back('"');
  }

  void AppendJsonField(
      std::string_view name,
      std::string_view value) { // NOLINT(bugprone-easily-swappable-parameters)
    _json_message.push_back(',');
    AppendJsonString(name);
    _json_message.push_back(':');
    AppendJsonString(value);
  }

  void generate_json_message(
      quill::MacroMetadata const *log_metadata, uint64_t log_timestamp,
      std::string_view thread_id, std::string_view /* thread_name */,
      std::string const & /* process_id */, std::string_view logger_name,
      quill::LogLevel /* log_level */, std::string_view log_level_description,
      std::string_view /* log_level_short_code */,
      std::vector<std::pair<std::string, std::string>> const *named_args,
      std::string_view log_message, std::string_view /* log_statement */,
      char const * /* message_format */) override {
    _json_message.push_back('{');
    AppendJsonString("timestamp");
    _json_message.push_back(':');
    AppendJsonString(std::to_string(log_timestamp));
    AppendJsonField(
        "file_name",
        std::filesystem::absolute(log_metadata->file_name()).string());
    AppendJsonField("line", log_metadata->line());
    AppendJsonField("thread_id", thread_id);
    AppendJsonField("logger", logger_name);
    AppendJsonField("log_level", log_level_description);
    AppendJsonField("message", log_message);

    if (named_args) {
      for (auto const &[key, value] : *named_args) {
        AppendJsonField(key, value);
      }
    }
  }
};

[[nodiscard]] inline quill::LogLevel ParseLogLevel(const std::string &level) {
  if (level == "DEBUG")
    return quill::LogLevel::Debug;
  if (level == "WARNING")
    return quill::LogLevel::Warning;
  if (level == "ERROR")
    return quill::LogLevel::Error;
  if (level == "CRITICAL")
    return quill::LogLevel::Critical;
  return quill::LogLevel::Info;
}

} // namespace ticketeer::core::logging
