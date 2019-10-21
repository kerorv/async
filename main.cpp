#include <ctime>
#include <iostream>
#include "app.h"
#include "callback.h"
#include "cotask.h"

#define STR(s) #s
#define STR2(s) STR(s)
#define LOCATION __FUNCTION__ "-" STR2(__LINE__)

class Object : public async::CallbackHost
{
public:
  Object(const char* location)
    : location_(location)
  {
    std::cout << ToString() << " constructor: " << this << std::endl;
  }
  ~Object() { std::cout << ToString() << " destructor: " << this << std::endl; }
  Object(const Object& o)
  {
    std::cout << ToString() << " copy constructor: " << this << " from " << &o
              << std::endl;
  }
  Object(Object&& o)
  {
    std::cout << ToString() << " move constructor: " << this << " from " << &o
              << std::endl;
  }
  Object& operator=(const Object&)
  {
    std::cout << ToString() << " copy assignment" << std::endl;
    return *this;
  }
  Object& operator=(Object&&)
  {
    std::cout << ToString() << " move assignment" << std::endl;
    return *this;
  }

  std::string ToString() { return std::string("Object ") + location_; }

  void OnEvent(int event)
  {
    std::cout << ToString() << " OnEvent: "
              << "event=" << event << std::endl;
  }

private:
  std::string location_;
};

async::CoTask<int> T1(Object obj)
{
  co_return 42;
}

async::CoTask<> T2()
{
  auto task = T1(Object(LOCATION));
  int res = co_await task;
  std::cout << "T1 return " << res << std::endl;
}

async::CoTask<> TestWhenAll1()
{
  std::vector<async::CoTask<int>> tasks;
  tasks.push_back(T1(Object(LOCATION)));
  tasks.push_back(T1(Object(LOCATION)));
  co_await async::WhenAll(std::move(tasks));
  std::cout << "All done" << std::endl;
}

async::CoTask<> TestWhenAll2()
{
  std::vector<async::CoTask<>> tasks;
  tasks.push_back(T2());
  tasks.push_back(T2());
  co_await async::WhenAll(std::move(tasks));
  std::cout << "All done" << std::endl;
}

void OnEvent2(int event)
{
  std::cout << "OnEvent2: event=" << event << std::endl;
}

class TimerThing : public async::CallbackHost
{
public:
  TimerThing(size_t id)
    : id_(id)
    , tick_(0)
  {
  }

  void OnTimer(TickTimerID timer_id)
  {
    std::cout << " : " << ++tick_ << " " << std::endl;
  }

private:
  size_t id_;
  size_t tick_;
};

int main()
{
  // async::CoSpawn(T2);
  // async::CoSpawn(TestWhenAll1);

  /*
    Object object(LOCATION);
    auto callback = MakeCallback(&Object::OnEvent, &object,
    std::placeholders::_1); callback.Invoke(5);

    auto callback2 = MakeCallback(&OnEvent2, std::placeholders::_1);
    callback2.Invoke(1);
*/
  /*
    auto timer_cb = MakeCallback(&OnTimer, std::placeholders::_1);

    using namespace std::chrono_literals;
    TickTimerManager::Instance().AddOneshotTimer(1s, timer_cb);
  */

  App app;

  auto th1 = std::make_unique<TimerThing>(1);
  auto th2 = std::make_unique<TimerThing>(2);
  auto th3 = std::make_unique<TimerThing>(3);
  app.AddPeriodTimer(
    std::chrono::hours{24}, MakeCallback(&TimerThing::OnTimer, th1.get(), std::placeholders::_1));
  app.AddPeriodTimer(
    std::chrono::seconds{60}, MakeCallback(&TimerThing::OnTimer, th2.get(), std::placeholders::_1));
  app.AddPeriodTimer(
    std::chrono::seconds{70}, MakeCallback(&TimerThing::OnTimer, th3.get(), std::placeholders::_1));

  app.Start();

  return 0;
}
