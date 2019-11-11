#include <ctime>
#include <iostream>
#include "app.h"
#include "callback.h"
#include "cotask.h"
#include "redis_client.h"
#include "resp_codec.h"
#include "result.h"

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

  void OnEvent(int n, int event)
  {
    std::cout << n << " -> Event: " << event << std::endl;
  }

private:
  size_t id_;
  size_t tick_;
};

void TestCo()
{
  async::CoSpawn(T2);
  async::CoSpawn(TestWhenAll1);
}

void TestCallback()
{
  Object object(LOCATION);
  auto callback = async::Bind<void(int)>(&Object::OnEvent, &object);
  callback.Invoke(5);

  auto callback2 = async::Bind<void(int)>(&OnEvent2);
  callback2.Invoke(1);
}

void TestCallbackEx()
{
  auto th1 = std::make_unique<TimerThing>(1);

  auto callback2 = async::Bind<void(int)>(&OnEvent2);
  callback2.Invoke(1);

  auto callback3 =
    async::Bind<void(TickTimerID)>(&TimerThing::OnTimer, th1.get());
  callback3.Invoke(TickTimerID{nullptr});

  auto callback4 = async::Bind<void(int)>(&TimerThing::OnEvent, th1.get(), 1);
  callback4.Invoke(4);
}

void TestAppTimer(App& app)
{
  auto th1 = std::make_unique<TimerThing>(1);
  auto th2 = std::make_unique<TimerThing>(2);
  auto th3 = std::make_unique<TimerThing>(3);

  app.AddPeriodTimer(std::chrono::hours{24},
    async::Bind<void(TickTimerID)>(&TimerThing::OnTimer, th1.get()));
  app.AddPeriodTimer(std::chrono::seconds{60},
    async::Bind<void(TickTimerID)>(&TimerThing::OnTimer, th2.get()));
  app.AddPeriodTimer(std::chrono::seconds{70},
    async::Bind<void(TickTimerID)>(&TimerThing::OnTimer, th3.get()));
}

void TestRESPCodec()
{
  RESPDecoder decoder;
  auto str = decoder.Decode("+OK\r\n");
  auto error = decoder.Decode("-Error message\r\n");
  auto integer = decoder.Decode(":1000\r\n");
  auto bulk_str = decoder.Decode("$10\r\nfoo \r\n bar\r\n");
  auto null_bulk_str = decoder.Decode("$-1\r\n");
  auto array = decoder.Decode("*2\r\n$3\r\nfoo\r\n$3\r\nbar\r\n");
  std::cout << ToString(array) << std::endl;
  auto array1 = decoder.Decode("*3\r\n:1\r\n:2\r\n:3\r\n");
  std::cout << ToString(array1) << std::endl;
  auto array2 =
    decoder.Decode("*5\r\n:1\r\n:2\r\n:3\r\n:4\r\n$6\r\nfoobar\r\n");
  std::cout << ToString(array2) << std::endl;
  auto null_array = decoder.Decode("*-1\r\n");
  std::cout << ToString(null_array) << std::endl;
  auto array3 =
    decoder.Decode("*2\r\n*3\r\n:1\r\n:2\r\n:3\r\n*2\r\n+Foo\r\n-Bar\r\n");
  std::cout << ToString(array3) << std::endl;
}

ZResult<RedisString> F0()
{
  return ZResult<RedisString>(std::string{"abc", 2});
}

ZResult<RedisMessage> F1()
{
  RedisString a{std::in_place_type<std::string>, "abc", 5};
  ResultValue<RedisString>(std::in_place_type<std::string>, "abc", 5);
  return F0();
}

void TestResult()
{
  F1();
}

class RedisClientConsole : public async::CallbackHost
{
public:
  void SetRedisClient(RedisClient* client) { client_ = client; }

  void OnConnected(int error)
  {
    std::cout << "redis client connected." << std::endl;

    client_->Command("*2\r\n$4\r\nkeys\r\n$1\r\n*\r\n",
      async::Bind<void(const ZResult<RedisMessage>&)>(
        &RedisClientConsole::OnCommand, this, "key *"));
  }

  void OnDisconnect()
  {
    std::cout << "redis client disconnected." << std::endl;
  }

  void OnCommand(const std::string& cmd, const ZResult<RedisMessage>& response)
  {
    std::cout << "command: " << cmd << " ";
    if (response)
    {
      std::cout << ToString(response.Value()) << std::endl;
    }
    else
    {
      std::cout << "error " << response.Error() << std::endl;
    }
  }

private:
  RedisClient* client_;
};

int main()
{
  App app;

  RedisClientConsole console;
  auto cb1 = async::Bind<void(int)>(&RedisClientConsole::OnConnected, &console);
  auto cb2 = async::Bind<void()>(&RedisClientConsole::OnDisconnect, &console);
  RedisClient client(app, cb1, cb2);
  console.SetRedisClient(&client);
  client.Connect(
    asio::ip::tcp::endpoint(asio::ip::address::from_string("10.0.2.30"), 6379));

  app.Start();

  return 0;
}
