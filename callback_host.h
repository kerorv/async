#pragma once

#include <cstdint>

namespace async
{
class LifeTimeTracker final
{
  struct SharedState;

public:
  class Monitor
  {
  public:
    Monitor()
      : state_(nullptr)
    {
    }

    explicit Monitor(SharedState* state)
      : state_(state)
    {
      AddCount();
    }

    Monitor(const Monitor& other)
      : state_(other.state_)
    {
      AddCount();
    }

    Monitor(Monitor&& other)
      : state_(other.state_)
    {
      other.state_ = nullptr;
    }

    ~Monitor() { DecCount(); }

    Monitor& operator=(const Monitor& other)
    {
      if (this == std::addressof(other))
      {
        return *this;
      }

      DecCount();
      state_ = other.state_;
      AddCount();

      return *this;
    }

    Monitor& operator=(Monitor&& other)
    {
      if (this == std::addressof(other))
      {
        return *this;
      }

      DecCount();
      state_ = other.state_;
      other.state_ = nullptr;

      return *this;
    }

    bool IsValid() const { return (state_ != nullptr); }
    bool IsAlive() const { return (state_ && state_->dead == 0); }

  private:
    void AddCount()
    {
      if (state_)
      {
        ++state_->count;
      }
    }

    void DecCount()
    {
      if (state_)
      {
        --state_->count;
        if (state_->count == 0 && state_->dead == 1)
        {
          delete state_;
          state_ = nullptr;
        }
      }
    }

  private:
    SharedState* state_;
  };

  LifeTimeTracker()
    : state_(new SharedState)
  {
  }

  LifeTimeTracker(const LifeTimeTracker&)
    : state_(new SharedState)
  {
  }

  LifeTimeTracker(LifeTimeTracker&&)
    : state_(new SharedState)
  {
  }

  ~LifeTimeTracker()
  {
    state_->dead = 1;
    if (state_->count == 0)
    {
      delete state_;
    }
  }

  LifeTimeTracker& operator=(const LifeTimeTracker&) { return *this; }

  LifeTimeTracker& operator=(const LifeTimeTracker&&) { return *this; }

  Monitor GetMonitor() { return Monitor(this->state_); }

private:
  struct SharedState
  {
    uint32_t count : 31;
    uint32_t dead : 1;

    SharedState()
      : count(0)
      , dead(0)
    {
    }
  };

  SharedState* state_;
};

class CallbackHost
{
public:
  LifeTimeTracker::Monitor GetMonitor() { return tracker_0_0_.GetMonitor(); }

private:
  LifeTimeTracker tracker_0_0_;
};
}  // namespace async
