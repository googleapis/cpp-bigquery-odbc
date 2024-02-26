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

#ifndef GOOGLE_CLOUD_ODBC_INTERNAL_STATUS_RECORD_OR_H
#define GOOGLE_CLOUD_ODBC_INTERNAL_STATUS_RECORD_OR_H

#include "google/cloud/odbc/internal/diagnostic_records.h"
#include "google/cloud/odbc/internal/odbc_includes.h"
#include "google/cloud/odbc/internal/sql_state_constants.h"
#include "absl/strings/match.h"
#include <optional>
#include <stdexcept>
#include <string>

namespace google::cloud::odbc_internal {

template <typename T>
class StatusRecordOr final {
 public:
  static_assert(!std::is_reference_v<T>,
                "StatusRecordOr<T> requires T to **not** be a reference type");

  /**
   * A `value_type` member for use in generic programming.
   *
   * This is analogous to that of `std::optional::value_type`.
   */
  using value_type = T;

  /**
   * Initializes with an error status (`StatusCode::kUnknown`).
   */
  StatusRecordOr() : StatusRecordOr(MakeDefaultStatusRecord()) {}

  ~StatusRecordOr() = default;

  StatusRecordOr(StatusRecordOr const&) = default;
  StatusRecordOr& operator=(StatusRecordOr const&) = default;
  StatusRecordOr(StatusRecordOr&& other) noexcept
      : status_record_(std::move(other.status_record_)),
        return_code_(std::move(other.return_code_)),
        value_(std::move(other.value_)) {
    other.status_record_ = MakeDefaultStatusRecord();
  }
  StatusRecordOr& operator=(StatusRecordOr&& other) noexcept {
    status_record_ = std::move(other.status_record_);
    return_code_ = std::move(other.return_code_);
    value_ = std::move(other.value_);
    other.status_record_ = MakeDefaultStatusRecord();
    return *this;
  }

  /**
   * Creates a new `StatusRecordOr<T>` holding the error condition @p rhs.
   *
   * @par Post-conditions
   * `Ok() == false` and `GetStatus() == rhs`.
   *
   * @param rhs the status to initialize the object.
   */
  // NOLINTNEXTLINE(google-explicit-constructor)
  StatusRecordOr(StatusRecord status) : status_record_(std::move(status)) {}
  StatusRecordOr(StatusRecord status, SQLRETURN return_code)
      : status_record_(std::move(status)),
        return_code_(std::move(return_code)) {}
  StatusRecordOr(std::optional<StatusRecord> status,
                 std::optional<SQLRETURN> return_code)
      : status_record_(std::move(status)),
        return_code_(std::move(return_code)) {}

  /**
   * Assigns the given StatusRecord to this `StatusRecordOr<T>`.
   *
   */
  StatusRecordOr& operator=(StatusRecord status_record) {
    *this = StatusRecordOr(std::move(status_record));
    return *this;
  }

  /**
   * Assign a `T` (or anything convertible to `T`) into the `StatusRecordOr`.
   *
   * This function does not participate in overload resolution if `U` is equal
   * to `StatusRecordOr<T>` (or to a cv-ref-qualified `StatusOr<T>`).
   *
   * @return a reference to this object.
   * @tparam U a type convertible to @p T.
   */
  template <typename U = T,
            /// @cond implementation_details
            std::enable_if_t<!std::is_same_v<StatusRecordOr, std::decay_t<U>>,
                             int> = 0
            /// @endcond
            >
  StatusRecordOr& operator=(U&& rhs) {
    status_record_.reset();
    return_code_.reset();
    value_ = std::forward<U>(rhs);
    return *this;
  }

  /**
   * Creates a new `StatusRecordOr<T>` holding the value @p rhs.
   *
   * @par Post-conditions
   * `Ok() == true` and `GetValue() == rhs`.
   *
   * @param rhs the value used to initialize the object.
   *
   * @throws ... If `T`'s move constructor throws.
   */
  // NOLINTNEXTLINE(google-explicit-constructor)
  StatusRecordOr(T&& rhs) : value_(std::move(rhs)) {}

  /**
   * Creates a new `StatusRecordOr<T>` holding the value @p rhs.
   *
   * @par Post-conditions
   * `Ok() == true` and `GetValue() == rhs`.
   *
   * @param rhs the value used to initialize the object.
   *
   * @throws ... If `T` copy constructor throws.
   */
  explicit StatusRecordOr(T const& rhs) : value_(rhs) {}

  /// Returns `true` when `this` holds a value.
  [[nodiscard]] bool Ok() const { return value_.has_value(); }

  /// Returns `true` when `this` holds a value.
  explicit operator bool() const { return value_.has_value(); }

  ///@{
  /**
   * @name Dereference operators.
   *
   * @par Pre-conditions
   * @parblock
   * `Ok() == true`
   *
   * @warning Using these operators when `Ok() == false` results in undefined
   *     behavior.
   * @endparblock
   *
   * @return A properly ref and const-qualified reference to the underlying
   *     value.
   */
  T& operator*() & { return *value_; }

  T const& operator*() const& { return *value_; }

  T&& operator*() && { return *std::move(value_); }

  T const&& operator*() const&& { return *std::move(value_); }
  ///@}

  ///@{
  /**
   * @name Member access operators.
   *
   * @par Pre-conditions
   * @parblock
   * `Ok() == true`
   *
   * @warning Using these operators when `Ok() == false` results in undefined
   *     behavior.
   * @endparblock
   *
   * @return A properly ref and const-qualified pointer to the underlying value.
   */
  T* operator->() & { return &*value_; }

  T const* operator->() const& { return &*value_; }
  ///@}

  ///@{
  /**
   * @name Value accessors.
   *
   * @return All these member functions return a (properly ref and
   *     const-qualified) reference to the underlying value.
   *
   * @throws RuntimeStatusError with the contents of `GetStatus()` if the object
   *   does not contain a value, i.e., if `Ok() == false`.
   */
  T& GetValue() & {
    CheckHasValue();
    return **this;
  }

  T const& GetValue() const& {
    CheckHasValue();
    return **this;
  }

  T&& GetValue() && {
    CheckHasValue();
    return std::move(**this);
  }

  T const&& GetValue() const&& {
    CheckHasValue();
    return std::move(**this);
  }
  ///@}

  ///@{
  /**
   * @name StatusRecord accessors.
   *
   * @par Pre-conditions
   * @parblock
   * `Ok() == false`
   *
   * @warning Using these operators when `Ok() == true` results in undefined
   *     behavior.
   *
   * @return A reference to the contained `StatusRecord`.
   */
  [[nodiscard]] StatusRecord const& GetStatusRecord() const& {
    return status_record_.value();
  }
  StatusRecord&& GetStatusRecord() && {
    return std::move(status_record_.value());
  }
  ///@}

  ///@{
  /**
   * @name ReturnCode accessors.
   *
   * @return An reference to the contained `std::optional<SQLRETURN>`.
   */
  [[nodiscard]] std::optional<SQLRETURN> const& GetReturnCode() const& {
    return return_code_;
  }
  std::optional<SQLRETURN>&& GetReturnCode() && {
    return std::move(return_code_);
  }
  ///@}

  /**
   * @return SQL_SUCCESS if 'Ok()', return_code_ if not empty, otherwise
   * calculate it based on status_record_.sql_state.
   */
  SQLRETURN GetCalculatedReturnCode() {
    if (Ok()) {
      return SQL_SUCCESS;
    }
    if (return_code_) {
      return return_code_.value();
    }
    if (absl::StartsWith(status_record_->sql_state, "01")) {
      return SQL_SUCCESS_WITH_INFO;
    }
    return SQL_ERROR;
  }

 private:
  static StatusRecord MakeDefaultStatusRecord() {
    return StatusRecord{kHY001, "default"};
  }

  void CheckHasValue() const& {
    if (!Ok()) {
      // TODO(215) inline after the new version of llvm-project is released
      std::string message = status_record_->message;
      throw std::runtime_error(message);
    }
  }
  void CheckHasValue() && {
    if (!Ok()) {
      // TODO(215) inline after the new version of llvm-project is released
      std::string message = status_record_->message;
      throw std::runtime_error(message);
    }
  }

  std::optional<StatusRecord> status_record_;
  std::optional<SQLRETURN> return_code_;
  std::optional<T> value_;
};

}  // namespace google::cloud::odbc_internal

#endif  // GOOGLE_CLOUD_ODBC_INTERNAL_STATUS_RECORD_OR_H
