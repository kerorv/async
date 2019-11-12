#pragma once

#include <array>
#include "callback.h"

struct TimerNode;
struct TickTimerID
{
  TimerNode* ptr;
};

using TickTimerCallback = async::Callback<void(TickTimerID)>;

struct TimerNode
{
  TimerNode* next;

  TickTimerCallback callback;
  size_t expire;
  uint32_t interval : 30;
  uint32_t periodic : 1;
  uint32_t valid : 1;

  TimerNode()
    : next(nullptr)
    , expire(0)
    , interval(0)
    , periodic(0)
    , valid(0)
  {
  }
};

class TickTimerWheel
{
public:
  TickTimerWheel(size_t size, size_t ticks_per_slot);
  void AddNode(TimerNode* node);
  void AddNodes(TimerNode* nodes, size_t current_tick);

  size_t MoveNext();
  TimerNode& CurrentSlot() { return slots_[cursor_]; }
  size_t SlotCount() const { return slots_.size(); }
  size_t WheelTicks() const { return slot_ticks_ * slots_.size(); }
  size_t Cursor() const { return cursor_; }

private:
  std::vector<TimerNode> slots_;
  size_t cursor_;
  const size_t slot_ticks_;
};

class TickTimerManager
{
public:
  TickTimerManager(std::initializer_list<size_t> il);
  void RunTick();

  TickTimerID AddPeriodTimer(
    uint32_t interval, const TickTimerCallback& callback);
  TickTimerID AddOneshotTimer(
    uint32_t delay, const TickTimerCallback& callback);
  void RemoveTimer(TickTimerID timer_id);

  size_t MaxTimerTicks() const { return wheels_.rbegin()->WheelTicks(); }

private:
  TickTimerID AddTimer(
    uint32_t interval, const TickTimerCallback& callback, bool is_periodic);
  void AddNode(TimerNode* node);
  void MoveWheel(size_t index);

  TimerNode* MakeNode();
  void RecycleNode(TimerNode* node);

private:
  std::vector<TickTimerWheel> wheels_;
  size_t tick_;
  TimerNode recycle_;
};
