#include "routes.hpp"

#include <json/json.h>
#include <libpq-fe.h>
#include <quill/Frontend.h>

#include "overlord.hpp"
#include "ticketeer/core/conf.hpp"
#include "utils.hpp"

namespace {

using Callback = std::function<void(const drogon::HttpResponsePtr &)>;

inline void BadRequest(const Callback &callback, const char *msg,
                       drogon::HttpStatusCode code = drogon::k400BadRequest) {
  Json::Value error;
  error["message"] = msg;
  auto resp = drogon::HttpResponse::newHttpJsonResponse(error);
  resp->setStatusCode(code);
  callback(resp);
}

[[nodiscard]] inline PGconn *ConnectDB() {
  return PQconnectdb(ticketeer::core::conf::settings.DB_URL.c_str());
}

} // namespace

namespace ticketeer::api {

void Auth::SignIn(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  auto *logger = quill::Frontend::get_logger("root");
  const auto body = req->getJsonObject();

  if (!body)
    return BadRequest(callback, "Invalid JSON body");

  const auto &username = (*body)["username"];
  const auto &password = (*body)["password"];

  if (username.isNull() || !username.isString() || password.isNull() ||
      !password.isString())
    return BadRequest(callback, "Missing username or password");

  PGconn *pg = ConnectDB();
  if (PQstatus(pg) != CONNECTION_OK) {
    PQfinish(pg);
    return BadRequest(callback, "Database unavailable",
                      drogon::k503ServiceUnavailable);
  }

  const auto user_id = auth::utils::Authenticate(
      logger, pg, username.asString(), password.asString());

  if (!user_id) {
    PQfinish(pg);
    return BadRequest(callback, "Invalid credentials",
                      drogon::k401Unauthorized);
  }

  const auto session = auth::utils::LogIn(logger, pg, *user_id);
  PQfinish(pg);

  if (!session)
    return BadRequest(callback, "Session creation failed",
                      drogon::k500InternalServerError);

  Json::Value result;
  result["token"] = session->token;
  result["expiry"] = session->expiry;
  auto resp = drogon::HttpResponse::newHttpJsonResponse(result);
  resp->addHeader("Authorization", "Bearer " + session->token);
  callback(resp);
}

void Auth::SignIn_Overlord(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  auto *logger = quill::Frontend::get_logger("root");
  PGconn *pg = ConnectDB();
  if (PQstatus(pg) != CONNECTION_OK) {
    PQfinish(pg);
    return BadRequest(callback, "Database unavailable",
                      drogon::k503ServiceUnavailable);
  }

  const auto user_id = auth::overlord::Authenticate(logger, pg, req);
  if (!user_id) {
    PQfinish(pg);
    return BadRequest(callback, "Missing session");
  }

  const auto session = auth::utils::LogIn(logger, pg, *user_id);
  PQfinish(pg);

  if (!session)
    return BadRequest(callback, "Session creation failed",
                      drogon::k500InternalServerError);

  Json::Value result;
  result["token"] = session->token;
  result["expiry"] = session->expiry;
  auto resp = drogon::HttpResponse::newHttpJsonResponse(result);
  resp->addHeader("Authorization", "Bearer " + session->token);
  callback(resp);
}

void Auth::Me(const drogon::HttpRequestPtr &req,
              std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  auto *logger = quill::Frontend::get_logger("root");
  const auto auth_header = req->getHeader("Authorization");
  if (auth_header.empty() || auth_header.rfind("Bearer ", 0) != 0)
    return BadRequest(callback, "Missing Authorization");

  const std::string token = auth_header.substr(7);

  PGconn *pg = ConnectDB();
  if (PQstatus(pg) != CONNECTION_OK) {
    PQfinish(pg);
    return BadRequest(callback, "Database unavailable",
                      drogon::k503ServiceUnavailable);
  }

  const auto username = auth::utils::FetchUsername(logger, pg, token);
  PQfinish(pg);

  if (!username)
    return BadRequest(callback, "Invalid or expired token",
                      drogon::k401Unauthorized);

  Json::Value result;
  result["username"] = *username;
  callback(drogon::HttpResponse::newHttpJsonResponse(result));
}

void Auth::SignOut(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  auto *logger = quill::Frontend::get_logger("root");
  const auto auth_header = req->getHeader("Authorization");
  if (auth_header.empty() || auth_header.rfind("Bearer ", 0) != 0)
    return BadRequest(callback, "Missing Authorization");

  const std::string token = auth_header.substr(7);

  PGconn *pg = ConnectDB();
  if (PQstatus(pg) != CONNECTION_OK) {
    PQfinish(pg);
    return BadRequest(callback, "Database unavailable",
                      drogon::k503ServiceUnavailable);
  }

  // First validate the token exists and is valid
  const auto username = auth::utils::FetchUsername(logger, pg, token);
  if (!username) {
    PQfinish(pg);
    return BadRequest(callback, "Invalid or expired token",
                      drogon::k401Unauthorized);
  }

  // Delete the session
  const bool deleted = auth::utils::SignOut(logger, pg, token);
  PQfinish(pg);

  if (!deleted) {
    return BadRequest(callback, "Logout failed",
                      drogon::k500InternalServerError);
  }

  Json::Value result;
  result["message"] = "Signed out successfully";
  callback(drogon::HttpResponse::newHttpJsonResponse(result));
}

} // namespace ticketeer::api
