#pragma once

#include <chrono>
#include "thirtyparty/asio/asio.hpp"
#include "tick_timer.h"

class App
{
public:
  App();
  void Start();

  TickTimerID AddPeriodTimer(
    std::chrono::seconds interval, const TickTimerCallback& callback);
  TickTimerID AddOneshotTimer(
    std::chrono::seconds delay, const TickTimerCallback& callback);
  void RemoveTimer(TickTimerID timer_id);

private:
  void OnTimer();

private:
  asio::io_context ioctx_;
  asio::steady_timer timer_;
  TickTimerManager ttm_;
  using TickPeriod =
    std::chrono::duration<long long, std::deci>;  // 1/10 second
};
