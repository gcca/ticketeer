#include "routes.hpp"

#include <json/json.h>
#include <quill/Frontend.h>
#include <sqlite3.h>

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

[[nodiscard]] inline sqlite3 *ConnectDB() {
  std::string path = ticketeer::core::conf::settings.DB_URL;
  if (path.starts_with("sqlite:"))
    path = path.substr(7);
  sqlite3 *db = nullptr;
  if (sqlite3_open(path.c_str(), &db) != SQLITE_OK) {
    sqlite3_close(db);
    return nullptr;
  }
  return db;
}

} // namespace

namespace ticketeer {

void Auth::SignInGet(const drogon::HttpRequestPtr &, Callback &&callback) {
  callback(drogon::HttpResponse::newHttpViewResponse("signin"));
}

void Auth::SignInPost(const drogon::HttpRequestPtr &req, Callback &&callback) {
  auto *logger = quill::Frontend::get_logger("root");

  const auto username = req->getParameter("username");
  const auto password = req->getParameter("password");

  if (username.empty() || password.empty())
    return BadRequest(callback, "Missing username or password");

  sqlite3 *db = ConnectDB();
  if (!db)
    return BadRequest(callback, "Database unavailable",
                      drogon::k503ServiceUnavailable);

  const auto user_id = handling::auth::utils::Authenticate(
      logger, db, username, password);

  if (!user_id) {
    sqlite3_close(db);
    return BadRequest(callback, "Invalid credentials",
                      drogon::k401Unauthorized);
  }

  auto resp = drogon::HttpResponse::newRedirectionResponse("/ticketeer/dashboard");
  const bool ok = handling::auth::utils::LogIn(logger, db, *user_id, resp);
  sqlite3_close(db);

  if (!ok)
    return BadRequest(callback, "Session creation failed",
                      drogon::k500InternalServerError);

  callback(resp);
}

// void Auth::SignIn_Overlord(
//     const drogon::HttpRequestPtr &req,
//     std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
//   auto *logger = quill::Frontend::get_logger("root");
//   sqlite3 *db = ConnectDB();
//   if (!db)
//     return BadRequest(callback, "Database unavailable",
//                       drogon::k503ServiceUnavailable);
//
//   const auto user_id = auth::overlord::Authenticate(logger, db, req);
//   if (!user_id) {
//     sqlite3_close(db);
//     return BadRequest(callback, "Missing session");
//   }
//
//   const auto session = handling::auth::utils::LogIn(logger, db, *user_id);
//   sqlite3_close(db);
//
//   if (!session)
//     return BadRequest(callback, "Session creation failed",
//                       drogon::k500InternalServerError);
//
//   Json::Value result;
//   result["token"] = session->token;
//   result["expiry"] = session->expiry;
//   auto resp = drogon::HttpResponse::newHttpJsonResponse(result);
//   resp->addHeader("Authorization", "Bearer " + session->token);
//   callback(resp);
// }

void Auth::SignOutPost(const drogon::HttpRequestPtr &req, Callback &&callback) {
  auto *logger = quill::Frontend::get_logger("root");
  const auto token = req->getCookie("token");
  if (token.empty())
    return callback(drogon::HttpResponse::newRedirectionResponse("/ticketeer/auth/signin"));

  sqlite3 *db = ConnectDB();
  if (!db)
    return BadRequest(callback, "Database unavailable",
                      drogon::k503ServiceUnavailable);

  const auto username = handling::auth::utils::FetchUsername(logger, db, token);
  if (!username) {
    sqlite3_close(db);
    return BadRequest(callback, "Invalid or expired token",
                      drogon::k401Unauthorized);
  }

  const bool deleted = handling::auth::utils::SignOut(logger, db, token);
  sqlite3_close(db);

  if (!deleted)
    return BadRequest(callback, "Logout failed",
                      drogon::k500InternalServerError);

  drogon::Cookie cookie("token", "");
  cookie.setHttpOnly(true);
  cookie.setPath("/");
  cookie.setExpiresDate(trantor::Date());

  auto resp = drogon::HttpResponse::newRedirectionResponse("/ticketeer/auth/signin");
  resp->addCookie(cookie);
  callback(resp);
}

} // namespace ticketeer
