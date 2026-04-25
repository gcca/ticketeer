#include "options.hpp"

#include <CLI11.hpp>

namespace ticketeer::core {

Options Options::Parse(int argc, char **argv) {
  CLI::App cli{"ticketeer-api"};
  Options opts;

  cli.add_option("-p,--port", opts.port, "Port to listen on")
      ->capture_default_str();

  cli.add_option("--log_level", opts.log_level,
                 "Log level (debug, info, warning, error, critical)")
      ->capture_default_str();

  cli.add_flag("--log_color", opts.log_color, "Colorize log output by level");
  cli.add_flag("--log_format", opts.log_format,
               "Pretty-print JSON log with space indentation");

  bool log_pretty = false;
  cli.add_flag("--log_pretty", log_pretty,
               "Shortcut for --log_color --log_format");

  auto *threading = cli.add_option_group("threading");
  threading->require_option(0, 1);
  threading->add_flag("--nothreaded", opts.nothreaded,
                      "Run on the calling thread only");
  threading->add_flag("--noconcurrency", opts.noconcurrency,
                      "Run with a single-thread pool (concurrency(1))");

  try {
    cli.parse(argc, argv);
  } catch (const CLI::ParseError &e) {
    throw OptionsParseError{e.what(), cli.exit(e)};
  }

  if (log_pretty) {
    opts.log_color = true;
    opts.log_format = true;
  }

  return opts;
}

} // namespace ticketeer::core
