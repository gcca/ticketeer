#pragma once

#include <drogon/HttpController.h>

namespace ticketeer::api {

class Auth : public drogon::HttpController<Auth> {
public:
  METHOD_LIST_BEGIN
  METHOD_ADD(Auth::SignIn, "/v1/signin", drogon::Post);
  METHOD_ADD(Auth::SignIn_Overlord, "/v1/signin/overlord", drogon::Post);
  METHOD_ADD(Auth::Me, "/v1/me", drogon::Get);
  METHOD_ADD(Auth::SignOut, "/v1/signout", drogon::Post);
  METHOD_LIST_END

  void SignIn(const drogon::HttpRequestPtr &req,
              std::function<void(const drogon::HttpResponsePtr &)> &&callback);

  void SignIn_Overlord(
      const drogon::HttpRequestPtr &req,
      std::function<void(const drogon::HttpResponsePtr &)> &&callback);

  void Me(const drogon::HttpRequestPtr &req,
          std::function<void(const drogon::HttpResponsePtr &)> &&callback);

  void SignOut(const drogon::HttpRequestPtr &req,
               std::function<void(const drogon::HttpResponsePtr &)> &&callback);
};

} // namespace ticketeer::api
