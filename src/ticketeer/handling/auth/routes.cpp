#include "routes.hpp"

#include <algorithm>
#include <cctype>
#include <string>

#include <drogon/DrTemplateBase.h>
#include <drogon/HttpClient.h>
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

[[nodiscard]] inline std::string Trim(std::string_view sv) {
  const auto start = sv.find_first_not_of(" \t\r\n");
  if (start == std::string_view::npos)
    return {};
  const auto end = sv.find_last_not_of(" \t\r\n");
  return std::string(sv.substr(start, end - start + 1));
}

[[nodiscard]] inline std::string UrlEncode(const std::string &s) {
  std::string out;
  for (unsigned char c : s) {
    if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~')
      out += static_cast<char>(c);
    else
      out += std::format("%{:02X}", c);
  }
  return out;
}

[[nodiscard]] inline std::string Base64Decode(const std::string &in) {
  static constexpr std::string_view chars =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string out;
  int val = 0, valb = -8;
  for (unsigned char c : in) {
    if (c == '=')
      break;
    const auto pos = chars.find(static_cast<char>(c));
    if (pos == std::string_view::npos)
      continue;
    val = (val << 6) + static_cast<int>(pos);
    valb += 6;
    if (valb >= 0) {
      out.push_back(static_cast<char>((val >> valb) & 0xFF));
      valb -= 8;
    }
  }
  return out;
}

[[nodiscard]] inline Json::Value DecodeJwtPayload(const std::string &token) {
  const auto dot1 = token.find('.');
  if (dot1 == std::string::npos)
    return {};
  const auto dot2 = token.find('.', dot1 + 1);
  if (dot2 == std::string::npos)
    return {};

  std::string payload = token.substr(dot1 + 1, dot2 - dot1 - 1);
  std::replace(payload.begin(), payload.end(), '-', '+');
  std::replace(payload.begin(), payload.end(), '_', '/');
  while (payload.size() % 4)
    payload += '=';

  const std::string decoded = Base64Decode(payload);
  Json::Value result;
  Json::Reader reader;
  reader.parse(decoded, result);
  return result;
}

[[nodiscard]] inline std::optional<std::uint64_t>
UpsertOAuthUser(quill::Logger *logger, sqlite3 *db, const std::string &username,
                const std::string &display_name) {
  std::mt19937_64 rng{std::random_device{}()};
  const std::string fake_pass =
      std::format("oauth:{:016x}{:016x}", rng(), rng());

  sqlite3_stmt *stmt = nullptr;
  const char *upsert_sql = R"SQL(
INSERT INTO auth_user (username, password, name, email)
VALUES (?, ?, ?, ?)
ON CONFLICT(username) DO UPDATE SET
  last_logged_in = CURRENT_TIMESTAMP,
  name  = excluded.name,
  email = excluded.email
RETURNING id
)SQL";

  if (sqlite3_prepare_v2(db, upsert_sql, -1, &stmt, nullptr) != SQLITE_OK) {
    LOGJ_DEBUG(logger, "[auth/app] upsert user error", sqlite3_errmsg(db));
    return std::nullopt;
  }

  const std::string display = display_name.empty() ? username : display_name;
  sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 2, fake_pass.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 3, display.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 4, username.c_str(), -1, SQLITE_STATIC);

  std::optional<std::uint64_t> user_id;
  if (sqlite3_step(stmt) == SQLITE_ROW)
    user_id = static_cast<std::uint64_t>(sqlite3_column_int64(stmt, 0));
  sqlite3_finalize(stmt);

  if (!user_id) {
    LOGJ_DEBUG(logger, "[auth/app] upsert returned no row");
    return std::nullopt;
  }

  const char *profile_sql = R"SQL(
INSERT INTO helpdesk_profile (user_id, role)
VALUES (?, 'requester')
ON CONFLICT(user_id) DO NOTHING
)SQL";

  if (sqlite3_prepare_v2(db, profile_sql, -1, &stmt, nullptr) != SQLITE_OK) {
    LOGJ_DEBUG(logger, "[auth/app] profile insert error", sqlite3_errmsg(db));
    return user_id;
  }

  sqlite3_bind_int64(stmt, 1, static_cast<sqlite3_int64>(*user_id));
  sqlite3_step(stmt);
  sqlite3_finalize(stmt);

  return user_id;
}

} // namespace

namespace ticketeer {

void Auth::SignInGet(const drogon::HttpRequestPtr &req, Callback &&callback) {
  const auto token = req->getCookie("token");

  if (not token.empty())
    return callback(
        drogon::HttpResponse::newRedirectionResponse("/ticketeer/dashboard"));

  drogon::HttpViewData data;
  data.insert("error", std::string{});
  callback(drogon::HttpResponse::newHttpViewResponse("signin", data));
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

  const auto user_id =
      handling::auth::utils::Authenticate(logger, db, username, password);

  if (!user_id) {
    sqlite3_close(db);
    drogon::HttpViewData data;
    data.insert("error", std::string("Usuario o contraseña incorrectos."));
    auto resp = drogon::HttpResponse::newHttpViewResponse("signin", data);
    resp->setStatusCode(drogon::k401Unauthorized);
    return callback(resp);
  }

  auto resp =
      drogon::HttpResponse::newRedirectionResponse("/ticketeer/dashboard");
  const bool ok = handling::auth::utils::LogIn(logger, db, *user_id, resp);
  sqlite3_close(db);

  if (!ok)
    return BadRequest(callback, "Session creation failed",
                      drogon::k500InternalServerError);

  callback(resp);
}

void Auth::SignOutPost(const drogon::HttpRequestPtr &req, Callback &&callback) {
  auto *logger = quill::Frontend::get_logger("root");
  const auto token = req->getCookie("token");
  if (token.empty())
    return callback(
        drogon::HttpResponse::newRedirectionResponse("/ticketeer/auth/signin"));

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
  cookie.setPath("/ticketeer");
  cookie.setExpiresDate(trantor::Date());

  auto resp =
      drogon::HttpResponse::newRedirectionResponse("/ticketeer/auth/signin");
  resp->addCookie(cookie);
  callback(resp);
}

void Auth::AppSignInGet(const drogon::HttpRequestPtr &, Callback &&callback) {
  auto *logger = quill::Frontend::get_logger("root");

  sqlite3 *db = ConnectDB();
  if (!db)
    return BadRequest(callback, "Database unavailable",
                      drogon::k503ServiceUnavailable);

  struct Provider {
    std::string client_id, tenant_id;
  };
  std::optional<Provider> provider;

  sqlite3_stmt *stmt = nullptr;
  const char *sql = "SELECT client_id, tenant_id FROM auth_app_provider ORDER "
                    "BY id DESC LIMIT 1";
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
    if (sqlite3_step(stmt) == SQLITE_ROW) {
      provider = Provider{
          .client_id =
              reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0)),
          .tenant_id =
              reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1)),
      };
    }
    sqlite3_finalize(stmt);
  }
  sqlite3_close(db);

  if (!provider) {
    LOGJ_DEBUG(logger, "[auth/app] no provider configured");
    return BadRequest(callback, "Office 365 login not configured",
                      drogon::k503ServiceUnavailable);
  }

  const std::string path =
      "/" + provider->tenant_id + "/oauth2/v2.0/devicecode";
  const std::string body =
      "client_id=" + provider->client_id + "&scope=openid+profile";

  auto client =
      drogon::HttpClient::newHttpClient("https://login.microsoftonline.com");
  auto msReq = drogon::HttpRequest::newHttpRequest();
  msReq->setMethod(drogon::Post);
  msReq->setPath(path);
  msReq->setBody(body);
  msReq->setCustomContentTypeString("application/x-www-form-urlencoded");

  client->sendRequest(
      msReq, [client, cb = std::move(callback),
              logger](drogon::ReqResult res,
                      const drogon::HttpResponsePtr &msResp) mutable {
        if (res != drogon::ReqResult::Ok) {
          LOGJ_DEBUG(logger, "[auth/app] device code request failed");
          auto resp = drogon::HttpResponse::newHttpResponse();
          resp->setStatusCode(drogon::k503ServiceUnavailable);
          resp->setBody("Microsoft API unavailable");
          return cb(resp);
        }

        Json::Value json;
        Json::Reader reader;
        reader.parse(std::string(msResp->getBody()), json);

        if (json.isMember("error")) {
          LOGJ_DEBUG(logger, "[auth/app] device code error",
                     json["error"].asString());
          auto resp = drogon::HttpResponse::newHttpResponse();
          resp->setStatusCode(drogon::k502BadGateway);
          resp->setBody("Device code request failed: " +
                        json.get("error_description", json["error"].asString())
                            .asString());
          return cb(resp);
        }

        const auto user_code = json["user_code"].asString();
        const auto device_code = json["device_code"].asString();
        if (user_code.empty() || device_code.empty()) {
          const std::string ms_body(msResp->getBody());
          LOGJ_DEBUG(logger, "[auth/app] empty user_code/device_code from MS",
                     ms_body);
          auto resp = drogon::HttpResponse::newHttpResponse();
          resp->setStatusCode(drogon::k502BadGateway);
          resp->setBody("Unexpected response from Microsoft (no user_code)");
          return cb(resp);
        }

        drogon::HttpViewData data;
        data.insert("user_code", user_code);
        data.insert("device_code", device_code);
        data.insert(
            "verification_uri",
            json.get("verification_uri", "https://microsoft.com/devicelogin")
                .asString());
        cb(drogon::HttpResponse::newHttpViewResponse("app_signin", data));
      });
}

void Auth::AppValidatePost(const drogon::HttpRequestPtr &req,
                           Callback &&callback) {
  auto *logger = quill::Frontend::get_logger("root");

  const auto jsonReq = req->getJsonObject();
  if (!jsonReq || !jsonReq->isMember("device_code")) {
    Json::Value j;
    j["status"] = "error";
    j["message"] = "Missing device_code";
    return callback(drogon::HttpResponse::newHttpJsonResponse(j));
  }
  const auto device_code = (*jsonReq)["device_code"].asString();
  if (device_code.empty()) {
    Json::Value j;
    j["status"] = "error";
    j["message"] = "Missing device_code";
    return callback(drogon::HttpResponse::newHttpJsonResponse(j));
  }

  sqlite3 *db = ConnectDB();
  if (!db) {
    Json::Value j;
    j["status"] = "error";
    j["message"] = "Database unavailable";
    return callback(drogon::HttpResponse::newHttpJsonResponse(j));
  }

  struct Creds {
    std::string client_id, client_secret, tenant_id, domain;
  };
  std::optional<Creds> creds;

  sqlite3_stmt *stmt = nullptr;
  const char *sql = "SELECT client_id, client_secret, tenant_id, domain FROM "
                    "auth_app_provider ORDER BY id DESC LIMIT 1";
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
    if (sqlite3_step(stmt) == SQLITE_ROW) {
      creds = Creds{
          .client_id = Trim(
              reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0))),
          .client_secret = Trim(
              reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1))),
          .tenant_id = Trim(
              reinterpret_cast<const char *>(sqlite3_column_text(stmt, 2))),
          .domain = Trim(
              reinterpret_cast<const char *>(sqlite3_column_text(stmt, 3))),
      };
    }
    sqlite3_finalize(stmt);
  }
  sqlite3_close(db);

  if (!creds) {
    Json::Value j;
    j["status"] = "error";
    j["message"] = "Provider not configured";
    return callback(drogon::HttpResponse::newHttpJsonResponse(j));
  }

  const std::string path = "/" + creds->tenant_id + "/oauth2/v2.0/token";
  const std::string msBody =
      "client_id=" + UrlEncode(creds->client_id) +
      "&client_secret=" + UrlEncode(creds->client_secret) +
      "&grant_type=urn%3Aietf%3Aparams%3Aoauth%3Agrant-type%3Adevice_code"
      "&device_code=" +
      UrlEncode(device_code);

  auto client =
      drogon::HttpClient::newHttpClient("https://login.microsoftonline.com");
  auto msReq = drogon::HttpRequest::newHttpRequest();
  msReq->setMethod(drogon::Post);
  msReq->setPath(path);
  msReq->setBody(msBody);
  msReq->setCustomContentTypeString("application/x-www-form-urlencoded");

  const std::string domain = creds->domain;
  client->sendRequest(
      msReq, [client, cb = std::move(callback), logger,
              domain](drogon::ReqResult res,
                      const drogon::HttpResponsePtr &msResp) mutable {
        if (res != drogon::ReqResult::Ok) {
          Json::Value j;
          j["status"] = "error";
          j["message"] = "Microsoft API unavailable";
          return cb(drogon::HttpResponse::newHttpJsonResponse(j));
        }

        Json::Value json;
        Json::Reader reader;
        reader.parse(std::string(msResp->getBody()), json);

        const auto error = json.get("error", "").asString();
        if (!error.empty()) {
          if (error == "authorization_pending" || error == "slow_down") {
            Json::Value j;
            j["status"] = "pending";
            return cb(drogon::HttpResponse::newHttpJsonResponse(j));
          }
          LOGJ_DEBUG(logger, "[auth/app] token error", error);
          Json::Value j;
          j["status"] = "error";
          j["message"] = json.get("error_description", error).asString();
          return cb(drogon::HttpResponse::newHttpJsonResponse(j));
        }

        const auto id_token = json.get("id_token", "").asString();
        if (id_token.empty()) {
          Json::Value j;
          j["status"] = "error";
          j["message"] = "No id_token in response";
          return cb(drogon::HttpResponse::newHttpJsonResponse(j));
        }

        const auto claims = DecodeJwtPayload(id_token);
        std::string username =
            claims.get("preferred_username", claims.get("unique_name", ""))
                .asString();
        if (username.empty())
          username = claims.get("email", "").asString();
        const std::string display_name =
            claims.get("name", username).asString();

        if (username.empty()) {
          Json::Value j;
          j["status"] = "error";
          j["message"] = "Could not determine username from token";
          return cb(drogon::HttpResponse::newHttpJsonResponse(j));
        }

        if (!domain.empty()) {
          const std::string suffix = "@" + domain;
          const bool allowed = username.size() > suffix.size() &&
                               username.compare(username.size() - suffix.size(),
                                                suffix.size(), suffix) == 0;
          if (!allowed) {
            Json::Value j;
            j["status"] = "error";
            j["message"] = "Email domain not authorized";
            return cb(drogon::HttpResponse::newHttpJsonResponse(j));
          }
        }

        sqlite3 *db = ConnectDB();
        if (!db) {
          Json::Value j;
          j["status"] = "error";
          j["message"] = "Database unavailable";
          return cb(drogon::HttpResponse::newHttpJsonResponse(j));
        }

        const auto user_id =
            UpsertOAuthUser(logger, db, username, display_name);
        if (!user_id) {
          sqlite3_close(db);
          Json::Value j;
          j["status"] = "error";
          j["message"] = "User provisioning failed";
          return cb(drogon::HttpResponse::newHttpJsonResponse(j));
        }

        Json::Value respJson;
        respJson["status"] = "ok";
        respJson["redirect"] = "/ticketeer/dashboard";
        auto resp = drogon::HttpResponse::newHttpJsonResponse(respJson);

        const bool ok =
            handling::auth::utils::LogIn(logger, db, *user_id, resp);
        sqlite3_close(db);

        if (!ok) {
          Json::Value j;
          j["status"] = "error";
          j["message"] = "Session creation failed";
          return cb(drogon::HttpResponse::newHttpJsonResponse(j));
        }

        cb(resp);
      });
}

} // namespace ticketeer
