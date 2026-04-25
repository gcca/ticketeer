#pragma once

#include <string>
#include <vector>

#include <gmock/gmock.h>

struct MockResult {
  bool ok{false};
  std::string error;
  std::vector<std::vector<std::string>> data;

  [[nodiscard]] int nrows() const { return (int)data.size(); }
  [[nodiscard]] const char *value(int r, int c) const {
    return data[r][c].c_str();
  }
};

struct MockDb {
  MOCK_METHOD(bool, connected, (), (const));
  MOCK_METHOD(std::string, conn_error, (), (const));
  MOCK_METHOD(MockResult, Exec, (const std::string &));
  MOCK_METHOD(MockResult, ExecParams,
              (const std::string &, const char *const *, int));
};

inline MockResult ok_result(std::vector<std::vector<std::string>> rows = {}) {
  return {true, "", std::move(rows)};
}

inline MockResult err_result(const std::string &msg = "db error") {
  return {false, msg, {}};
}
