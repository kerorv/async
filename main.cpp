#include <iostream>
#include "cotask.h"

#define STR(s) #s
#define STR2(s) STR(s)
#define LOCATION __FUNCTION__ "-" STR2(__LINE__)

class Object
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

  std::string ToString()
  {
    return std::string("Object ")+location_;
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

int main()
{
  //  async::CoSpawn(T2);
  async::CoSpawn(TestWhenAll1);
  return 0;
}
