#pragma once

#include <functional>
#include "callback_host.h"

template <typename T>
class Callback
{
public:
  Callback() = default;

  explicit Callback(std::function<T> func)
    : func_(std::move(func))
  {
  }

  Callback(std::function<T> func, async::LifeTimeTracker::Monitor monitor)
    : func_(std::move(func))
    , monitor_(std::move(monitor))
  {
  }

  template <typename... Args>
  void Invoke(Args&&... args) const
  {
    if (!func_)
    {
      return;
    }

    if (monitor_.IsValid() && !monitor_.IsAlive())
    {
      return;
    }

    func_(std::forward<Args>(args)...);
  }

private:
  std::function<T> func_;
  async::LifeTimeTracker::Monitor monitor_;
};

namespace async
{
namespace detail
{
template <int... I, typename F, typename... Args>
auto MyBind(std::integer_sequence<int, I...>, F&& f, Args&&... args)
{
  return std::bind(
    std::forward<F>(f), std::forward<Args>(args)..., std::_Ph<I + 1>{}...);
}

template <typename T>
struct FunctionSignature;
template <typename R, typename... Args>
struct FunctionSignature<R(Args...)>
{
  static constexpr size_t argnum = sizeof...(Args);
};

template <typename F>
struct Binder;

template <typename R, typename... Params>
struct Binder<R (*)(Params...)>
{
  static_assert((!std::is_pointer_v<Params> && ...));

  template <typename T, typename... Args>
  static Callback<T> Bind(R (*f)(Params...), Args&&... args)
  {
    static_assert(
      sizeof...(Params) == sizeof...(Args) + FunctionSignature<T>::argnum);
    return Callback<T>(
      MyBind(std::make_integer_sequence<int, FunctionSignature<T>::argnum>(), f,
        std::forward<Args>(args)...));
  }
};

template <typename R, typename C, typename... Params>
struct Binder<R (C::*)(Params...)>
{
  static_assert((!std::is_pointer_v<Params> && ...));
  static_assert(std::is_base_of_v<async::CallbackHost, C>);

  template <typename T, typename... Args>
  static Callback<T> Bind(R (C::*mf)(Params...), C* clsptr, Args&&... args)
  {
    static_assert(
      sizeof...(Params) == sizeof...(Args) + FunctionSignature<T>::argnum);
    return Callback<T>(
      MyBind(std::make_integer_sequence<int, FunctionSignature<T>::argnum>(),
        mf, clsptr, std::forward<Args>(args)...),
      static_cast<async::CallbackHost*>(clsptr)->GetMonitor());
  }
};
}  // namespace detail

template <typename T, typename F, typename... Args>
Callback<T> Bind(F&& f, Args&&... args)
{
  return detail::Binder<F>::template Bind<T>(f, std::forward<Args>(args)...);
}
}  // namespace async
