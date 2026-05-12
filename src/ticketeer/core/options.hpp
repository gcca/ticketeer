#pragma once

#include <cstdint>
#include <cstdlib>
#include <string>

#include <CLI11.hpp>

namespace ticketeer::core::options {

struct ServerOptions {
  std::string bind = "0.0.0.0";
  std::uint16_t port = 5521;
  std::string log_level = "INFO";
};

[[nodiscard]] inline ServerOptions InitServerOptions(int argc, char *argv[]) {
  ServerOptions options;

  CLI::App cli{"ticketeer-api"};

  cli.add_option("-b,--bind", options.bind, "Address to bind")
      ->default_val(options.bind);

  cli.add_option("-p,--port", options.port, "Port to listen on")
      ->default_val(options.port)
      ->check(CLI::Range(1, 65535));

  cli.add_option("--log_level", options.log_level, "Log level")
      ->default_val(options.log_level)
      ->check(CLI::IsMember({"DEBUG", "INFO", "WARNING", "ERROR", "CRITICAL"}));

  try {
    cli.parse(argc, argv);
  } catch (const CLI::ParseError &error) {
    std::exit(cli.exit(error));
  }

  return options;
}

} // namespace ticketeer::core::options
