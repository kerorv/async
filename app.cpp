#include "app.h"

App::App()
  : timer_(ioctx_)
  , ttm_{600, 60, 24}
{
}

void App::Start()
{
  timer_.expires_from_now(TickPeriod{1});
  timer_.async_wait([this](const std::error_code& ec) { OnTimer(); });
  ioctx_.run();
}

TickTimerID App::AddPeriodTimer(
  std::chrono::seconds interval, const TickTimerCallback& callback)
{
  uint32_t interval_tick = std::max<uint32_t>(interval / TickPeriod{1}, 1);
  interval_tick = std::min<size_t>(interval_tick, ttm_.MaxTimerTicks());
  return ttm_.AddPeriodTimer(interval_tick, callback);
}

TickTimerID App::AddOneshotTimer(
  std::chrono::seconds delay, const TickTimerCallback& callback)
{
  uint32_t delay_tick = std::max<uint32_t>(delay / TickPeriod{1}, 1);
  delay_tick = std::min<size_t>(delay_tick, ttm_.MaxTimerTicks());
  return ttm_.AddOneshotTimer(delay_tick, callback);
}

void App::RemoveTimer(TickTimerID timer_id)
{
  ttm_.RemoveTimer(timer_id);
}

void App::OnTimer()
{
  ttm_.RunTick();

  timer_.expires_at(timer_.expiry() + TickPeriod{1});
  timer_.async_wait([this](const std::error_code& ec) { OnTimer(); });
}
