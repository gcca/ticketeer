#include <cstdlib>
#include <exception>
#include <iostream>
#include <string>

#include <libpq-fe.h>

#include "CLI11.hpp"

int main(int argc, char **argv) {
  try {
    CLI::App cli{"create-user"};

    std::string username;
    std::string password;
    std::string connstr = "host=localhost dbname=ticketeer";

    cli.add_option("username", username, "Username")->required();
    cli.add_option("password", password, "Password")->required();
    cli.add_option("-c,--connection", connstr, "PostgreSQL connection string")
        ->envname("DATABASE_URL");

    CLI11_PARSE(cli, argc, argv);

    PGconn *conn = PQconnectdb(connstr.c_str());
    if (PQstatus(conn) != CONNECTION_OK) {
      std::cerr << PQerrorMessage(conn);
      PQfinish(conn);
      return 1;
    }

    const char *params[] = {username.c_str(), password.c_str()};
    PGresult *res =
        PQexecParams(conn,
                     "INSERT INTO ticketeer.auth_user (username, password)"
                     " VALUES ($1, crypt($2, gen_salt('bf')))",
                     2, nullptr, params, nullptr, nullptr, 0);

    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
      std::cerr << PQerrorMessage(conn);
      PQclear(res);
      PQfinish(conn);
      return 1;
    }

    PQclear(res);
    PQfinish(conn);
    return 0;
  } catch (const std::exception &ex) {
    std::cerr << ex.what() << '\n';
    return 1;
  } catch (...) {
    std::cerr << "Unknown error\n";
    return 1;
  }
}
