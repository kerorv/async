#pragma once

#include <experimental/coroutine>
#include <optional>
#include "callback_host.h"

namespace async
{
template <typename T>
class CoTask;

namespace detail
{
template <typename... Args>
struct SelectFirst;

template <typename T, typename... Args>
struct SelectFirst<T, Args...>
{
  using type = T;
};

template <typename... Args>
async::CallbackHost* Filter(async::CallbackHost* v, Args const&... args)
{
  static_assert(
    ((!std::is_pointer_v<Args> && !std::is_reference_v<Args>)&&...));
  return v;
}

class CoFrameBase
{
public:
  CoFrameBase();

  bool Resume();

  void Destroy();

  void DestroyChain();

  void SetPrevFrame(CoFrameBase* frame) { prev_frame_ = frame; }
  CoFrameBase* GetPrevFrame() const { return prev_frame_; }

  template <typename... Args>
  void* operator new(size_t size, Args const&... args)
  {
    // *** BROKEN: May not get called if heap allocation is elided.
    host = nullptr;
    if constexpr (sizeof...(Args) > 0)
    {
      if constexpr (std::is_pointer_v<typename SelectFirst<Args...>::type>)
      {
        host = Filter(args...);
      }
      else
      {
        static_assert(
          ((!std::is_pointer_v<Args> && !std::is_reference_v<Args>)&&...));
      }
    }
    return ::operator new(size);
  }

  void operator delete(void* pointer, std::size_t size)
  {
    ::operator delete(pointer);
  }

protected:
  std::experimental::coroutine_handle<> coroutine_ = nullptr;
  async::LifeTimeTracker::Monitor monitor_;

  CoFrameBase* prev_frame_ = nullptr;

private:
  static async::CallbackHost* host;
};

struct FinalAwaitable
{
  bool await_ready() const { return false; }

  template <typename P>
  void await_suspend(std::experimental::coroutine_handle<P> coroutine)
  {
    CoFrameBase& current_frame = coroutine.promise();
    CoFrameBase* prev_frame = current_frame.GetPrevFrame();
    if (prev_frame == nullptr)
    {
      return;
    }

    if (!prev_frame->Resume())
    {
      current_frame.DestroyChain();
    }
  }

  void await_resume() {}
};

template <typename T>
class CoFrame : public CoFrameBase
{
public:
  CoFrame()
  {
    this->coroutine_ =
      std::experimental::coroutine_handle<CoFrame>::from_promise(*this);
  }

  auto initial_suspend() { return std::experimental::suspend_always{}; }

  auto final_suspend() { return FinalAwaitable{}; }

  CoTask<T> get_return_object();

  void unhandled_exception() { throw; }

  template <typename U>
  void return_value(U&& value)
  {
    result = std::forward<U>(value);
  }

  T& Result() { return result.value(); }

#ifdef PROMISE_CONSTRUCTOR_ACCEPT_COROUTINE_PARAMTERS
  // MSVC don't support Promise constructor accept coroutine parameters
  // http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2018/p0914r1.html
  template <typename C, typename... Args>
  CoFrame(C* clsptr, Args&&... args)
  {
    static_assert(false);
  }
#endif

private:
  std::optional<T> result;
};

template <>
class CoFrame<void> : public CoFrameBase
{
public:
  CoFrame()
  {
    this->coroutine_ =
      std::experimental::coroutine_handle<CoFrame>::from_promise(*this);
  }

  auto initial_suspend() { return std::experimental::suspend_always{}; }

  auto final_suspend() { return FinalAwaitable{}; }

  CoTask<void> get_return_object();

  void unhandled_exception() { throw; }

  void return_void() {}

  void Result() {}
};
}  // namespace detail

template <typename T = void>
class CoTask final
{
public:
  using promise_type = detail::CoFrame<T>;

  explicit CoTask(promise_type* frame)
    : frame_(frame)
  {
  }

  CoTask(CoTask&& other)
  {
    frame_ = other.frame_;
    other.frame_ = nullptr;
  }

  CoTask() = delete;
  CoTask(const CoTask&) = delete;
  CoTask& operator=(const CoTask&) = delete;

  CoTask& operator=(CoTask&& other)
  {
    if (this == std::addressof(other))
    {
      return *this;
    }

    frame_ = other.frame_;
    other.frame_ = nullptr;
    return *this;
  }

  ~CoTask()
  {
    if (frame_)
    {
      frame_->Destroy();
    }
  }

  bool await_ready() const { return false; }

  template <typename U>
  void await_suspend(std::experimental::coroutine_handle<U> coroutine)
  {
    detail::CoFrameBase& prev_frame = coroutine.promise();
    frame_->SetPrevFrame(&prev_frame);
    frame_->Resume();
  }

  decltype(auto) await_resume() { return frame_->Result(); }

private:
  promise_type* frame_;
};

namespace detail
{
template <typename T>
CoTask<T> CoFrame<T>::get_return_object()
{
  return CoTask<T>(this);
}

inline CoTask<void> CoFrame<void>::get_return_object()
{
  return CoTask<void>(this);
}
}  // namespace detail

template <typename T>
struct IsCoTask : public std::false_type
{
};

template <typename T>
struct IsCoTask<CoTask<T>> : public std::true_type
{
};

namespace detail
{
struct DetachedTask
{
  class promise_type : public CoFrameBase
  {
  public:
    DetachedTask get_return_object()
    {
      this->coroutine_ =
        std::experimental::coroutine_handle<promise_type>::from_promise(*this);
      return {};
    }

    std::experimental::suspend_never initial_suspend() { return {}; }

    std::experimental::suspend_never final_suspend() { return {}; }

    void return_void() {}
  };
};

template <typename T>
DetachedTask AwaitTask(CoTask<T> task)
{
  co_await task;
}
}  // namespace detail

template <typename F, typename... Args>
void CoSpawn(F&& f, Args&&... args)
{
  static_assert(IsCoTask<typename std::result_of<F(args...)>::type>::value,
    "CoSpawn accept function parameter that return CoTask.");
  // TODO: check all args must not be pointer or reference

  detail::AwaitTask(f(std::forward<Args>(args)...));
}

template <typename R, typename C, typename... Args>
void CoSpawn(R C::*const mf, C* clsptr, Args... args)
{
  static_assert(IsCoTask<decltype((clsptr->*mf)(args...))>::value,
    "CoSpawn accept function parameter that return CoTask.");
  // TODO: check all args must not be pointer or reference
  static_assert((!std::is_pointer_v<Args> && ...));

  detail::AwaitTask((clsptr->*mf)(args...));
}
}  // namespace async
