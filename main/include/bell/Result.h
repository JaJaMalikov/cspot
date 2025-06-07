#pragma once
#include <optional>
#include <string>
#include <system_error>

namespace bell {

// Simple Result type inspired by std::expected
template <typename T = void>
class Result {
 public:
  Result() = default;
  Result(const T& v) : value(v) {}
  Result(T&& v) : value(std::move(v)) {}
  Result(std::error_code ec) : error(ec) {}

  explicit operator bool() const { return !error.has_value(); }

  const T& getValue() const { return value.value(); }
  T& getValue() { return value.value(); }
  T takeValue() { T tmp = std::move(value.value()); value.reset(); return tmp; }

  std::error_code getError() const { return error.value(); }
  std::string errorMessage() const { return error ? error->message() : ""; }

 private:
  std::optional<T> value;
  std::optional<std::error_code> error;
};

// void specialization
template <>
class Result<void> {
 public:
  Result() = default;
  Result(std::error_code ec) : error(ec) {}

  operator bool() const { return !error.has_value(); }
  std::error_code getError() const { return error.value(); }
  std::string errorMessage() const { return error ? error->message() : ""; }

 private:
  std::optional<std::error_code> error;
};

}  // namespace bell
