#pragma once

#include <cstdint>
#include <string_view>
#include <variant>
#include <vector>

using RedisInteger = int64_t;
using RedisString = std::variant<nullptr_t, std::string>;
using RedisError = std::string;

struct RedisArray
{
  using Element =
    std::variant<RedisInteger, RedisString, RedisError, RedisArray>;
  using Elements = std::vector<Element>;
  std::variant<nullptr_t, Elements> array;
};

using RedisMessage =
  std::variant<RedisInteger, RedisString, RedisError, RedisArray>;

class RESPEncoder
{
public:
  std::string Encode(const RedisMessage& msg);
};

class RESPDecoder
{
public:
  RedisMessage Decode(std::string_view sv);

private:
  RedisString DecodeSimpleStr(std::string_view& sv);
  RedisError DecodeError(std::string_view& sv);
  int64_t DecodeInteger(std::string_view& sv);
  RedisString DecodeBulkStr(std::string_view& sv);
  RedisArray DecodeArray(std::string_view& sv);
  RedisMessage InternalDecode(std::string_view& sv);
};

std::string ToString(const RedisMessage& msg);
