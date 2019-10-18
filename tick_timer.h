#pragma once

#include <array>
#include "callback.h"

struct TimerNode;
struct TickTimerID
{
  TimerNode* ptr;
};

using TickTimerCallback = Callback<void(TickTimerID)>;

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
  TickTimerWheel(size_t size);
  void AddNode(TimerNode* node, size_t offset_tick);
  void AddNodes(TimerNode* nodes, size_t current_tick);

  size_t MoveNext();
  TimerNode& CurrentSlot() { return slots_[cursor_]; }
  size_t SlotCount() const { return slots_.size(); }

private:
  std::vector<TimerNode> slots_;
  size_t cursor_;
};

class TickTimerManager
{
public:
  TickTimerManager(std::initializer_list<size_t> il);
  void RunTick();

  TickTimerID AddPeriodTimer(uint32_t interval, const TickTimerCallback& callback);
  TickTimerID AddOneshotTimer(uint32_t delay, const TickTimerCallback& callback);
  void RemoveTimer(TickTimerID timer_id);

private:
  TickTimerID AddTimer(uint32_t interval, const TickTimerCallback& callback, bool is_periodic);
  void AddNode(TimerNode* node);
  void MoveWheel(size_t index);

  TimerNode* MakeNode();
  void RecycleNode(TimerNode* node);

private:
  std::vector<TickTimerWheel> wheels_;
  size_t tick_;
  TimerNode recycle_;
};
