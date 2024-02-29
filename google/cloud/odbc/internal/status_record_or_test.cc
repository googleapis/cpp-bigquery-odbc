// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/odbc/internal/status_record_or.h"
#include "google/cloud/odbc/internal/sql_state_constants.h"
#include <gtest/gtest.h>

namespace google::cloud::odbc_internal {

TEST(StatusRecordOrTest, ValueType) {
  struct Foo {};
  static_assert(std::is_same_v<StatusRecordOr<int>::value_type, int>);
  static_assert(std::is_same_v<StatusRecordOr<char>::value_type, char>);
  static_assert(std::is_same_v<StatusRecordOr<Foo>::value_type, Foo>);

  static_assert(!std::is_same_v<StatusRecordOr<int>::value_type, char>);
  static_assert(!std::is_same_v<StatusRecordOr<char>::value_type, Foo>);
  static_assert(!std::is_same_v<StatusRecordOr<Foo>::value_type, int>);
}

TEST(ConstructorTest, DefaultConstructor) {
  StatusRecordOr<int> actual;

  EXPECT_FALSE(actual.Ok());
  EXPECT_FALSE(actual);
  EXPECT_FALSE(actual.GetStatusRecord().sql_state.empty());
}

TEST(ConstructorTest, StatusRecordConstructor) {
  StatusRecordOr<int> actual(StatusRecord{SQLStates::k_01000(), "message"});

  EXPECT_FALSE(actual.Ok());
  EXPECT_FALSE(actual);
  EXPECT_EQ(SQLStates::k_01000(), actual.GetStatusRecord().sql_state);
  EXPECT_EQ("message", actual.GetStatusRecord().message);
}

TEST(ConstructorTest, StatusRecordAndReturnCodeConstructor) {
  StatusRecordOr<int> actual(StatusRecord{SQLStates::k_01000(), "message"},
                             SQL_NO_DATA);

  EXPECT_FALSE(actual.Ok());
  EXPECT_FALSE(actual);
  EXPECT_EQ(SQLStates::k_01000(), actual.GetStatusRecord().sql_state);
  EXPECT_EQ("message", actual.GetStatusRecord().message);
  EXPECT_EQ(SQL_NO_DATA, actual.GetCalculatedReturnCode());
}

TEST(AssignmentOperator, StatusAssignment) {
  StatusRecord status_record{SQLStates::k_01000(), "message"};
  StatusRecordOr<int> actual;

  actual = status_record;

  EXPECT_FALSE(actual);
  EXPECT_EQ(SQLStates::k_01000(), actual.GetStatusRecord().sql_state);
}

TEST(ConstructorTest, ValueConstructor) {
  StatusRecordOr<int> actual(42);

  EXPECT_TRUE(actual);
  EXPECT_EQ(42, actual.GetValue());
  EXPECT_EQ(42, std::move(actual).GetValue());
}

TEST(ConstructorTest, ValueConstAccessors) {
  StatusRecordOr<int> const actual(42);

  EXPECT_TRUE(actual);
  EXPECT_EQ(42, actual.GetValue());
  EXPECT_EQ(42, std::move(actual).GetValue());
}

TEST(ConstructorTest, NewClassOfAnotherTypeSuccess) {
  StatusRecordOr<char> error(StatusRecord{SQLStates::k_01000(), "message"});
  EXPECT_FALSE(error);

  StatusRecordOr<int> same_error(error.GetStatusRecord(),
                                 error.GetReturnCode());
  EXPECT_FALSE(same_error);
  EXPECT_EQ(SQLStates::k_01000(), same_error.GetStatusRecord().sql_state);
  EXPECT_FALSE(same_error.GetReturnCode().has_value());
}

TEST(ValueAccessors, ThrowError) {
  StatusRecord status_record{SQLStates::k_01000(), "message"};
  StatusRecordOr<int> actual(status_record);

  EXPECT_THROW(actual.GetValue(), std::runtime_error);
}

TEST(ValueAccessors, ThrowError_AfterMove) {
  StatusRecord status_record{SQLStates::k_01000(), "message"};
  StatusRecordOr<int> actual(status_record);

  EXPECT_THROW(std::move(actual).GetValue(), std::runtime_error);
}

TEST(StatusRecordAccessors, StatusConstAccessors) {
  StatusRecord status_record{SQLStates::k_01000(), "message"};
  StatusRecordOr<int> const actual(status_record);

  EXPECT_FALSE(actual);
  EXPECT_EQ(SQLStates::k_01000(), actual.GetStatusRecord().sql_state);
  EXPECT_EQ("message", actual.GetStatusRecord().message);
}

TEST(ValueDeference, ValueDeference) {
  StatusRecordOr<std::string> actual("val");
  EXPECT_TRUE(actual);
  EXPECT_EQ("val", *actual);
  EXPECT_EQ("val", std::move(actual).GetValue());
}

TEST(ValueDeference, ValueDeference_Const) {
  StatusRecordOr<std::string> const actual("val");
  EXPECT_TRUE(actual);
  EXPECT_EQ("val", *actual);
  EXPECT_EQ("val", std::move(actual).GetValue());
}

TEST(ValueDeference, ValueDeference_Arrow) {
  StatusRecordOr<std::string> actual("val");
  EXPECT_TRUE(actual);
  EXPECT_EQ(std::string("val"), actual->c_str());
}

TEST(ValueDeference, ValueDeference_ConstArrow) {
  StatusRecordOr<std::string> const actual("val");
  EXPECT_TRUE(actual);
  EXPECT_EQ(std::string("val"), actual->c_str());
}

TEST(StatusRecordOr, MovedFromState) {
  StatusRecordOr<int> a(123);
  EXPECT_TRUE(a);

  // Asserts that a moved-from StatusRecordOr is equal to a default constructed
  // one.
  auto b = std::move(a);
  StatusRecordOr<int> default_object;
  EXPECT_EQ(a.GetStatusRecord().sql_state,
            default_object.GetStatusRecord().sql_state);
  a = std::move(b);
  EXPECT_EQ(b.GetStatusRecord().sql_state,
            default_object.GetStatusRecord().sql_state);
}

TEST(StatusRecordOr, AssignmentNotAmbiguous) {
  StatusRecordOr<std::string> actual(std::string{"42"});
  EXPECT_TRUE(actual);
  EXPECT_EQ("42", *actual);
  actual = "7";
  EXPECT_TRUE(actual);
  EXPECT_EQ("7", *actual);
  actual = StatusRecordOr<std::string>("42");
  EXPECT_TRUE(actual);
  EXPECT_EQ("42", *actual);
}

TEST(ReturnCode, Success) {
  StatusRecordOr<std::string> actual(std::string{"42"});

  EXPECT_EQ(SQL_SUCCESS, actual.GetCalculatedReturnCode());
}

TEST(ReturnCode, ReturnAssigned) {
  StatusRecordOr<std::string> actual({SQLStates::k_01000(), "message"},
                                     SQL_NO_DATA);

  EXPECT_EQ(SQL_NO_DATA, actual.GetCalculatedReturnCode());
}

TEST(ReturnCode, ReturnNotAssigned_SQL_SUCCESS_WITH_INFO) {
  StatusRecordOr<std::string> actual(
      StatusRecord{SQLStates::k_01000(), "message"});

  EXPECT_EQ(SQL_SUCCESS_WITH_INFO, actual.GetCalculatedReturnCode());
}

TEST(ReturnCode, ReturnNotAssigned_SQL_ERROR) {
  StatusRecordOr<std::string> actual(
      StatusRecord{SQLStates::k_42000(), "message"});

  EXPECT_EQ(SQL_ERROR, actual.GetCalculatedReturnCode());
}

TEST(ReturnCode, ReturnNotAssigned_EmptyCode) {
  StatusRecordOr<std::string> actual(StatusRecord{"", "message"});

  EXPECT_EQ(SQL_ERROR, actual.GetCalculatedReturnCode());
}

StatusRecordOr<std::string> ReturnString() {
  std::string a = "ok";
  return a;
}

StatusRecordOr<std::string> ReturnFailure() {
  return StatusRecord{SQLStates::k_42000(), "message"};
}

TEST(ImplicitConversion, ReturnSuccess) {
  StatusRecordOr<std::string> success = ReturnString();
  EXPECT_TRUE(success);
}

TEST(ImplicitConversion, ReturnFailure) {
  StatusRecordOr<std::string> success = ReturnFailure();
  EXPECT_FALSE(success);
}

TEST(ConvertFromStatusOr, Success) {
  StatusOr<std::string> expected("value");

  StatusRecordOr<std::string> actual =
      StatusRecordOr<std::string>::ConvertFromStatusOr(expected);

  EXPECT_TRUE(actual);
  EXPECT_EQ("value", *actual);
}

TEST(ConvertFromStatusOr, Failure) {
  StatusOr<std::string> expected(
      Status(StatusCode::kInvalidArgument, "message"));

  StatusRecordOr<std::string> actual =
      StatusRecordOr<std::string>::ConvertFromStatusOr(expected);

  EXPECT_FALSE(actual);
  EXPECT_EQ(SQLStates::k_42000(), actual.GetStatusRecord().sql_state);
  EXPECT_EQ("[BigQuery] message", actual.GetStatusRecord().message);
}

/// A class without a default constructor.
class NoDefaultConstructor {
 public:
  NoDefaultConstructor() = delete;
  explicit NoDefaultConstructor(std::string x) : str_(std::move(x)) {}

  [[nodiscard]] std::string str() const { return str_; }

 private:
  std::string str_;
};

TEST(StatusRecordOrNoDefaultConstructor, DefaultConstructed) {
  StatusRecordOr<NoDefaultConstructor> empty;
  EXPECT_FALSE(empty.Ok());
}

TEST(StatusRecordOrNoDefaultConstructor, ValueConstructed) {
  StatusRecordOr<NoDefaultConstructor> actual(
      NoDefaultConstructor(std::string("foo")));
  EXPECT_TRUE(actual);
  EXPECT_EQ(actual->str(), "foo");
}

}  // namespace google::cloud::odbc_internal
