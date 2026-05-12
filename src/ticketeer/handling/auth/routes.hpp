#pragma once

#include <drogon/HttpController.h>

namespace ticketeer {

class Auth : public drogon::HttpController<Auth> {
public:
  METHOD_LIST_BEGIN
  METHOD_ADD(Auth::SignInGet, "/signin", drogon::Get);
  METHOD_ADD(Auth::SignInPost, "/signin", drogon::Post);
  METHOD_ADD(Auth::SignOutPost, "/signout", drogon::Post,
             "ticketeer::handling::auth::middlewares::LogInRequired");
  METHOD_ADD(Auth::AppSignInGet, "/app/signin", drogon::Get);
  METHOD_ADD(Auth::AppValidatePost, "/app/validate", drogon::Post);
  METHOD_LIST_END

  void
  SignInGet(const drogon::HttpRequestPtr &req,
            std::function<void(const drogon::HttpResponsePtr &)> &&callback);

  void
  SignInPost(const drogon::HttpRequestPtr &req,
             std::function<void(const drogon::HttpResponsePtr &)> &&callback);

  void
  SignOutPost(const drogon::HttpRequestPtr &req,
              std::function<void(const drogon::HttpResponsePtr &)> &&callback);

  void
  AppSignInGet(const drogon::HttpRequestPtr &req,
               std::function<void(const drogon::HttpResponsePtr &)> &&callback);

  void AppValidatePost(
      const drogon::HttpRequestPtr &req,
      std::function<void(const drogon::HttpResponsePtr &)> &&callback);
};

} // namespace ticketeer
