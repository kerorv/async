#include "tick_timer.h"

////////////////////////////////////////////////////////////////////
// TickTimerWheel
TickTimerWheel::TickTimerWheel(size_t size)
  : cursor_(0)
{
  slots_.resize(size);
}

void TickTimerWheel::AddNode(TimerNode* node, size_t offset_tick)
{
  size_t slot_idx = (cursor_ + offset_tick) % SlotCount();
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
    AddNode(node, (node->expire - current_tick) % SlotCount());
    node = node->next;
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
  wheels_.reserve(il.size());
  for (auto slot_count : il)
  {
    wheels_.push_back(TickTimerWheel(slot_count));
  }
}

TickTimerID TickTimerManager::AddPeriodTimer(uint32_t interval, const TickTimerCallback& callback)
{
  return AddTimer(interval, callback, true);
}

TickTimerID TickTimerManager::AddOneshotTimer(uint32_t delay, const TickTimerCallback& callback)
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
  size_t offset_tick = node->expire - tick_;
  size_t wheel_tick = 1;
  for (auto& wheel : wheels_)
  {
    wheel_tick *= wheel.SlotCount();
    if (offset_tick / wheel_tick == 0)
    {
      wheel.AddNode(node, offset_tick);
      return;
    }
  }
}

void TickTimerManager::RunTick()
{
  ++tick_;
  MoveWheel(0);

  auto& slot = wheels_[0].CurrentSlot();
  TimerNode* node = slot.next;
  while (node)
  {
    auto next = node->next;
    if (node->valid)
    {
      node->callback.Invoke(TickTimerID{node});
    }

    if (!node->valid || !node->periodic)
    {
      // recycle timer
      RecycleNode(node);
    }
    else
    {
      // add timer again
      node->expire += node->interval;
      AddNode(node);
    }

    node = next;
  }
  slot.next = nullptr;
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

    if (index > 0)
    {
      auto slot = wheels_[index].CurrentSlot();
      // move slot to prev wheel
      wheels_[index - 1].AddNodes(slot.next, tick_);
    }
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
