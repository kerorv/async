#pragma once

#include <variant>

template <typename T, bool E>
struct ResultPart
{
  ResultPart() = default;
  template <typename U, typename... Args,
    typename = std::enable_if_t<std::is_convertible_v<U, T> ||
                                std::is_constructible_v<T, U, Args...>>>
  ResultPart(U&& ty, Args&&... args)
    : part(std::forward<U>(ty), std::forward<Args>(args)...)
  {
  }

  T part{};
};

template <typename T>
using ResultValue = ResultPart<T, false>;

template <typename T>
using ResultError = ResultPart<T, true>;

template <typename V, typename T = std::decay_t<V>>
ResultValue<T> Success(V&& v)
{
  return {std::forward<V>(v)};
}

template <typename E, typename T = std::decay_t<E>>
ResultError<T> Failure(E&& e)
{
  return {std::forward<E>(e)};
}

template <typename V, typename E>
class Result
{
public:
  using ValueType = ResultValue<V>;
  using ErrorType = ResultError<E>;

  Result() = default;

  Result(ValueType&& value)
    : res_(value)
  {
  }

  Result(ErrorType&& error)
    : res_(error)
  {
  }

  template <typename U,
    typename = std::enable_if_t<std::is_convertible_v<U, V> ||
                                std::is_constructible_v<V, U>>>
  Result(Result<U, E>&& r)
  {
    using ArgType = Result<U, E>;
    if (r)
    {
      res_ = ValueType(r.Value());
    }
    else
    {
      res_ = ErrorType(r.Error());
    }
  }

  operator bool() const { return std::holds_alternative<ValueType>(res_); }

  const V& Value() const { return std::get<ValueType>(res_).part; }

  const E& Error() const { return std::get<ErrorType>(res_).part; }

private:
  std::variant<ValueType, ErrorType> res_;
};

template <typename E>
class Result<void, E>
{
public:
  using ErrorType = ResultError<E>;

  Result()
    : error_{}
  {
  }

  Result(ErrorType&& error)
    : error_(error)
  {
  }

  const E& Error() const { return error_.error; }

private:
  ErrorType error_;
};

template <typename T>
using ZResult = Result<T, int>;

using ZError = Result<void, int>;
