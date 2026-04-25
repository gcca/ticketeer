#include "log.hpp"

#include <chrono>
#include <cstdio>
#include <ctime>
#include <string>

#include <quill/Backend.h>
#include <quill/Frontend.h>
#include <quill/core/LogLevel.h>
#include <quill/sinks/JsonSink.h>

namespace ticketeer::core {

namespace {

quill::Logger *g_logger = nullptr;

void JsonEscape(fmtquill::memory_buffer &buf, std::string_view s) {
  for (char c : s) {
    if (c == '"')
      buf.append(std::string_view{"\\\""});
    else if (c == '\\')
      buf.append(std::string_view{"\\\\"});
    else if (c == '\n')
      buf.append(std::string_view{"\\n"});
    else if (c == '\r')
      buf.append(std::string_view{"\\r"});
    else
      buf.push_back(c);
  }
}

class StructlogConsoleSink : public quill::JsonConsoleSink {
public:
  explicit StructlogConsoleSink(bool color, bool indent)
      : _kc{color ? "\033[36m" : ""}, _vc{color ? "\033[33m" : ""},
        _rs{color ? "\033[0m" : ""}, _sep{indent ? "\n  " : ""},
        _colon{indent ? ": " : ":"}, _comma{indent ? ",\n  " : ","} {}

  QUILL_ATTRIBUTE_HOT void
  write_log(quill::MacroMetadata const *macro_metadata, uint64_t log_timestamp,
            std::string_view thread_id, std::string_view thread_name,
            std::string const &process_id, std::string_view logger_name,
            quill::LogLevel log_level, std::string_view log_level_description,
            std::string_view log_level_short_code,
            std::vector<std::pair<std::string, std::string>> const *named_args,
            std::string_view log_message,
            std::string_view log_statement) override {
    generate_json_message(macro_metadata, log_timestamp, thread_id, thread_name,
                          process_id, logger_name, log_level,
                          log_level_description, log_level_short_code,
                          named_args, log_message, log_statement, nullptr);
    _json_message.push_back('}');
    _json_message.push_back('\n');

    std::fwrite(_json_message.data(), 1, _json_message.size(), stdout);
    _json_message.clear();
  }

  QUILL_ATTRIBUTE_HOT void generate_json_message(
      quill::MacroMetadata const *, uint64_t log_timestamp, std::string_view,
      std::string_view, std::string const &, std::string_view logger_name,
      quill::LogLevel, std::string_view log_level_description, std::string_view,
      std::vector<std::pair<std::string, std::string>> const *named_args,
      std::string_view log_message, std::string_view, char const *) override {

    auto tp = std::chrono::system_clock::time_point{
        std::chrono::duration_cast<std::chrono::system_clock::duration>(
            std::chrono::nanoseconds{log_timestamp})};
    auto secs = std::chrono::floor<std::chrono::seconds>(tp);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(tp - secs)
                  .count();
    auto tt = std::chrono::system_clock::to_time_t(secs);
    std::tm tm{};
    localtime_r(&tt, &tm);
    char ts[24];
    std::strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%S", &tm);

    std::string level{log_level_description};
    for (char &c : level)
      c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    auto AppendKey = [&](std::string_view key) {
      _json_message.append(_kc);
      _json_message.push_back('"');
      _json_message.append(key);
      _json_message.push_back('"');
      _json_message.append(_rs);
      _json_message.append(_colon);
    };

    auto AppendValue = [&](std::string_view value, bool escape = false) {
      _json_message.append(_vc);
      _json_message.push_back('"');
      if (escape)
        JsonEscape(_json_message, value);
      else
        _json_message.append(value);
      _json_message.push_back('"');
      _json_message.append(_rs);
    };

    _json_message.push_back('{');
    _json_message.append(_sep);
    AppendKey("timestamp");
    AppendValue(fmtquill::format("{}.{:03d}", ts, ms));
    _json_message.append(_comma);
    AppendKey("level");
    AppendValue(level);
    _json_message.append(_comma);
    AppendKey("logger");
    AppendValue(logger_name);
    _json_message.append(_comma);
    AppendKey("event");
    AppendValue(log_message, true);

    if (named_args) {
      for (auto const &[key, value] : *named_args) {
        _json_message.append(_comma);
        AppendKey(key);
        AppendValue(value, true);
      }
    }

    if (!_sep.empty())
      _json_message.push_back('\n');
  }

private:
  std::string_view _kc;
  std::string_view _vc;
  std::string_view _rs;
  std::string_view _sep;
  std::string_view _colon;
  std::string_view _comma;
};

quill::LogLevel ParseLevel(const std::string &level) {
  return quill::loglevel_from_string(level);
}

} // namespace

void SetupLogging(const std::string &level, bool color, bool indent) {
  quill::Backend::start();

  auto sink = quill::Frontend::create_or_get_sink<StructlogConsoleSink>(
      "structlog", color, indent);

  g_logger =
      quill::Frontend::create_or_get_logger("api-master", std::move(sink));
  g_logger->set_log_level(ParseLevel(level));
}

quill::Logger *logger() { return g_logger; }

} // namespace ticketeer::core
