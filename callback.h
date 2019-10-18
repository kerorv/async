#pragma once

#include <functional>
#include "callback_host.h"

template <typename T>
struct Callback
{
  std::function<T> func;
  async::LifeTimeTracker::Monitor monitor;

  template <typename... Args>
  void Invoke(Args&&... args)
  {
    if (!func)
    {
      return;
    }

    if (monitor.IsValid() && !monitor.IsAlive())
    {
      return;
    }

    func(std::forward<Args>(args)...);
  }
};

template <typename T>
struct CallbackFunctionTraits;

template <typename R, typename... Args>
struct CallbackFunctionTraits<R(Args...)>
{
  static_assert((!std::is_pointer_v<Args> && ...));
};

template <typename R, typename... Args>
struct CallbackFunctionTraits<R (*)(Args...)>
  : public CallbackFunctionTraits<R(Args...)>
{
  static_assert((!std::is_pointer_v<Args> && ...));

  using CallType = R(Args...);
  static const size_t argnum = sizeof...(Args);
  static const bool is_member_func = false;
};

template <typename R, typename C, typename... Args>
struct CallbackFunctionTraits<R (C::*)(Args...)>
  : public CallbackFunctionTraits<R(C&, Args...)>
{
  static_assert(std::is_base_of_v<async::CallbackHost, C>);
  static_assert((!std::is_pointer_v<Args> && ...));

  using CallType = R(Args...);
  using ClassType = C;
  static const size_t argnum = sizeof...(Args);
  static const bool is_member_func = true;
};

template <typename R, typename C, typename... Args>
struct CallbackFunctionTraits<R (C::*)(Args...) const>
  : public CallbackFunctionTraits<R(C&, Args...)>
{
  static_assert(std::is_base_of_v<async::CallbackHost, C>);
  static_assert((!std::is_pointer_v(Args) && ...));

  using CallType = R(Args...);
  using ClassType = C;
  static const size_t argnum = sizeof...(Args);
  static const bool is_member_func = true;
};

template <typename... Args>
async::CallbackHost* CallbackHostFilter(
  async::CallbackHost* v, Args const&... args)
{
  //  static_assert(
  //    ((!std::is_pointer_v<Args> && !std::is_reference_v<Args>)&&...));
  return v;
}

template <typename F, typename... Args>
auto MakeCallback(F&& f, Args&&... args)
  -> Callback<typename CallbackFunctionTraits<F>::CallType>
{
  async::LifeTimeTracker::Monitor monitor;
  if constexpr (CallbackFunctionTraits<F>::is_member_func)
  {
    static_assert(std::is_base_of_v<async::CallbackHost,
      CallbackFunctionTraits<F>::ClassType>);
    async::CallbackHost* host = CallbackHostFilter(std::forward<Args>(args)...);
    monitor = host->GetMonitor();
  }

  return {std::function<typename CallbackFunctionTraits<F>::CallType>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)),
    monitor};
}
