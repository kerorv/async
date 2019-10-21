#include "tick_timer.h"
#include <iostream>

////////////////////////////////////////////////////////////////////
// TickTimerWheel
TickTimerWheel::TickTimerWheel(size_t size, size_t ticks_per_slot)
  : cursor_(0)
  , slot_ticks_(ticks_per_slot)
{
  slots_.resize(size);
}

void TickTimerWheel::AddNode(TimerNode* node)
{
  size_t expire = node->expire % WheelTicks();
  size_t slot_idx = expire / slot_ticks_;
  TimerNode& slot = slots_[slot_idx];
  // insert node
  node->next = slot.next;
  slot.next = node;
}

void TickTimerWheel::AddNodes(TimerNode* nodes, size_t current_tick)
{
  TimerNode* node = nodes;
  while (node)
  {
    TimerNode* next = node->next;
    AddNode(node);
    node = next;
  }
}

size_t TickTimerWheel::MoveNext()
{
  cursor_ = (cursor_ + 1) % SlotCount();
  return cursor_;
}

////////////////////////////////////////////////////////////////////
// TickTimerManager
TickTimerManager::TickTimerManager(std::initializer_list<size_t> il)
  : tick_(0)
{
  size_t ticks_per_slot = 1;
  wheels_.reserve(il.size());
  for (auto slot_count : il)
  {
    wheels_.push_back(TickTimerWheel(slot_count, ticks_per_slot));
    ticks_per_slot *= slot_count;
  }
}

TickTimerID TickTimerManager::AddPeriodTimer(
  uint32_t interval, const TickTimerCallback& callback)
{
  return AddTimer(interval, callback, true);
}

TickTimerID TickTimerManager::AddOneshotTimer(
  uint32_t delay, const TickTimerCallback& callback)
{
  return AddTimer(delay, callback, false);
}

void TickTimerManager::RemoveTimer(TickTimerID timer_id)
{
  auto node = timer_id.ptr;
  if (node)
  {
    node->valid = false;
  }
}

TickTimerID TickTimerManager::AddTimer(
  uint32_t interval, const TickTimerCallback& callback, bool is_periodic)
{
  TimerNode* node = MakeNode();
  node->interval = interval;
  node->expire = tick_ + interval;
  node->callback = callback;
  node->periodic = is_periodic;
  node->valid = true;

  AddNode(node);

  return TickTimerID{node};
}

void TickTimerManager::AddNode(TimerNode* node)
{
  for (auto& wheel : wheels_)
  {
    if (node->interval <= wheel.WheelTicks())
    {
      wheel.AddNode(node);
      return;
    }
  }
}

void TickTimerManager::RunTick()
{
  ++tick_;
  MoveWheel(0);

  TimerNode wait_add_nodes;
  auto& slot = wheels_[0].CurrentSlot();
  TimerNode* node = slot.next;
  while (node)
  {
    auto next = node->next;

    if (node->valid)
    {
      std::cout << "timer(" << node->interval << ") [0][" << wheels_[0].Cursor()
                << "] invoke";
      node->callback.Invoke(TickTimerID{node});
    }

    if (!node->valid || !node->periodic)
    {
      // recycle timer
      RecycleNode(node);
    }
    else
    {
      // update timer expire
      node->expire += node->interval;

      // append to waiting list
      node->next = wait_add_nodes.next;
      wait_add_nodes.next = node;
    }

    node = next;
  }

  // clear the slot
  slot.next = nullptr;

  // add periodic node
  node = wait_add_nodes.next;
  while (node)
  {
    auto next = node->next;
    AddNode(node);
    node = next;
  }
}

void TickTimerManager::MoveWheel(size_t index)
{
  if (wheels_[index].MoveNext() == 0)
  {
    if (index + 1 < wheels_.size())
    {
      // move next wheel
      MoveWheel(index + 1);
    }
  }

  if (index > 0)
  {
    std::cout << "move [" << index << "][" << wheels_[index].Cursor()
              << "] to prev-wheel" << std::endl;
    // move next slot into prev-wheel
    auto& slot = wheels_[index].CurrentSlot();  // ???
    wheels_[index - 1].AddNodes(slot.next, tick_);
    slot.next = nullptr;
  }
}

TimerNode* TickTimerManager::MakeNode()
{
  TimerNode* node = recycle_.next;
  if (node == nullptr)
  {
    return new TimerNode;
  }

  recycle_.next = node->next;
  return node;
}

void TickTimerManager::RecycleNode(TimerNode* node)
{
  node->next = recycle_.next;
  recycle_.next = node;
}
