#include <iostream>
#include "cotask.h"

class Object
{
public:
  Object() { std::cout << "Object constructor: " << this << std::endl; }
  ~Object() { std::cout << "Object deconstructor: " << this << std::endl; }
  Object(const Object& o)
  {
    std::cout << "Object copy constructor: " << this << " from " << &o
              << std::endl;
  }
  Object(Object&& o)
  {
    std::cout << "Object move constructor: " << this << " from " << &o
              << std::endl;
  }
  Object& operator=(const Object&)
  {
    std::cout << "Object copy assignment" << std::endl;
    return *this;
  }
  Object& operator=(Object&&)
  {
    std::cout << "Object move assignment" << std::endl;
    return *this;
  }
};

async::CoTask<int> T1(Object obj)
{
  co_return 42;
}

async::CoTask<> T2()
{
//  Object obj;
  auto task = T1(Object());
  co_await task;
}

int main()
{
  async::CoSpawn(T2);
  return 0;
}
