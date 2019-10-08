#include "cotask.h"

using namespace async::detail;

async::CallbackHost* CoFrameBase::host = nullptr;

CoFrameBase::CoFrameBase()
{
  if (host)
  {
    monitor_ = host->GetMonitor();
  }
}

bool CoFrameBase::Resume()
{
  if (coroutine_ == nullptr)
  {
    return false;
  }

  if (monitor_.IsValid() && !monitor_.IsAlive())
  {
    return false;
  }

  coroutine_.resume();
  return true;
}

void CoFrameBase::Destroy()
{
  coroutine_.destroy();
}

void CoFrameBase::DestroyChain()
{
  if (prev_frame_)
  {
    prev_frame_->DestroyChain();
  }

  if (coroutine_)
  {
    coroutine_.destroy();
  }
}
