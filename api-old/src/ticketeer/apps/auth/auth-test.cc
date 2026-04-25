#include <gtest/gtest.h>

#include "ticketeer/apps/auth/internal.hpp"
#include "ticketeer/apps/auth/mock-db.hpp"

namespace {

using ::testing::_;
using ::testing::Return;

crow::request MakeRequest(const std::string &body) {
  crow::request req;
  req.body = body;
  return req;
}

ticketeer::core::Context kContext;
const crow::request kValidReq =
    MakeRequest(R"({"username":"alice","password":"secret"})");

TEST(SignInTest, MissingFields_Returns400) {
  MockDb db;
  EXPECT_EQ(ticketeer::apps::auth::SignIn(MakeRequest("{}"), db, kContext).code,
            400);
}

TEST(SignInTest, MissingPassword_Returns400) {
  MockDb db;
  EXPECT_EQ(ticketeer::apps::auth::SignIn(MakeRequest(R"({"username":"alice"})"),
                                         db, kContext)
                .code,
            400);
}

TEST(SignInTest, AuthQueryFails_Returns503) {
  MockDb db;
  EXPECT_CALL(db, ExecParams(_, _, _))
      .WillOnce(Return(err_result("conn error")));
  EXPECT_EQ(ticketeer::apps::auth::SignIn(kValidReq, db, kContext).code, 503);
}

TEST(SignInTest, InvalidCredentials_Returns401) {
  MockDb db;
  EXPECT_CALL(db, ExecParams(_, _, _)).WillOnce(Return(ok_result()));
  EXPECT_EQ(ticketeer::apps::auth::SignIn(kValidReq, db, kContext).code, 401);
}

TEST(SignInTest, SessionInsertFails_Returns503) {
  MockDb db;
  EXPECT_CALL(db, ExecParams(_, _, _))
      .WillOnce(Return(ok_result({{"42"}})))
      .WillOnce(Return(err_result()));
  EXPECT_EQ(ticketeer::apps::auth::SignIn(kValidReq, db, kContext).code, 503);
}

TEST(SignInTest, ValidCredentials_Returns200WithToken) {
  MockDb db;
  EXPECT_CALL(db, ExecParams(_, _, _))
      .WillOnce(Return(ok_result({{"42"}})))
      .WillOnce(Return(ok_result()));
  ticketeer::core::settings.auth_expiration_seconds = 3600;
  const auto res = ticketeer::apps::auth::SignIn(kValidReq, db, kContext);
  EXPECT_EQ(res.code, 200);
  EXPECT_NE(res.body.find("\"token\":\""), std::string::npos);
}

} // namespace
