#include "resp_codec.h"
#include <charconv>
#include <sstream>

// https://redis.io/topics/protocol

#define SIMPLE_STR_PREFIX '+'
#define ERROR_PREFIX '-'
#define INTEGER_PREFIX ':'
#define BULK_STR_PREFIX '$'
#define ARRAY_PREFIX '*'

///////////////////////////////////////////////////////////////////////////////
// RESPEncoder
std::string RESPEncoder::Encode(const RedisMessage& msg)
{
  return ToString(msg);
}

///////////////////////////////////////////////////////////////////////////////
// RESPDecoder
RedisMessage RESPDecoder::Decode(std::string_view sv)
{
  return InternalDecode(sv);
}

RedisMessage RESPDecoder::InternalDecode(std::string_view& sv)
{
  switch (sv[0])
  {
    case SIMPLE_STR_PREFIX:
    {
      return DecodeSimpleStr(sv);
    }
    break;
    case ERROR_PREFIX:
    {
      return DecodeError(sv);
    }
    break;
    case INTEGER_PREFIX:
    {
      return DecodeInteger(sv);
    }
    break;
    case BULK_STR_PREFIX:
    {
      return DecodeBulkStr(sv);
    }
    break;
    case ARRAY_PREFIX:
    {
      return DecodeArray(sv);
    }
    break;
    default:
      break;
  }
}

RedisString RESPDecoder::DecodeSimpleStr(std::string_view& sv)
{
  auto pos = sv.find_first_of("\r\n");
  RedisString ret = std::string{sv.substr(1, pos - 1)};
  sv.remove_prefix(pos + 2);
  return ret;
}

RedisError RESPDecoder::DecodeError(std::string_view& sv)
{
  auto pos = sv.find_first_of("\r\n");
  RedisError ret{sv.substr(1, pos - 1)};
  sv.remove_prefix(pos + 2);
  return ret;
}

int64_t RESPDecoder::DecodeInteger(std::string_view& sv)
{
  int64_t ret;
  auto pos = sv.find_first_of("\r\n");
  std::from_chars(sv.data() + 1, sv.data() + pos, ret);
  sv.remove_prefix(pos + 2);
  return ret;
}

RedisString RESPDecoder::DecodeBulkStr(std::string_view& sv)
{
  std::string_view sv_copy(sv);
  auto pos = sv_copy.find_first_of("\r\n");

  int len = 0;
  std::from_chars(sv_copy.data() + 1, sv_copy.data() + pos, len);
  if (len == -1)
  {
    // null string
    return {nullptr};
  }

  sv.remove_prefix(pos + 2 + len + 2);
  return {std::string(sv_copy.data() + pos + 2, len)};
}

RedisArray RESPDecoder::DecodeArray(std::string_view& sv)
{
  std::string_view sv_copy(sv);
  auto pos = sv_copy.find_first_of("\r\n");

  int len = 0;
  std::from_chars(sv_copy.data() + 1, sv_copy.data() + pos, len);
  if (len == -1)
  {
    // null array
    return {nullptr};
  }

  sv.remove_prefix(pos + 2);

  RedisArray ret{RedisArray::Elements(len)};
  for (int i = 0; i < len; ++i)
  {
    RedisArray::Element ele = InternalDecode(sv);
    std::get<1>(ret.array)[i] = ele;
  }

  return ret;
}

static void InternalToString(const RedisMessage& msg, std::ostringstream& oss)
{
  switch (msg.index())
  {
    case 0:  // integer
    {
      oss << "Integer: " << std::get<0>(msg) << "\n";
    }
    break;
    case 1:  // string
    {
      const RedisString& rs = std::get<1>(msg);
      switch (rs.index())
      {
        case 0:
          oss << "String: nil\n";
          break;
        case 1:
          oss << "String: " << std::get<1>(rs) << "\n";
          break;
        default:
          oss << "String: unknow type.\n";
          break;
      }
    }
    break;
    case 2:  // error
    {
      const RedisError& re = std::get<2>(msg);
      oss << "Error: " << re << "\n";
    }
    break;
    case 3:  // array
    {
      const RedisArray& ra = std::get<3>(msg);
      switch (ra.array.index())
      {
        case 0:
        {
          oss << "Array: nil\n";
        }
        break;
        case 1:
        {
          const auto& elements = std::get<1>(ra.array);
          oss << "Array[" << elements.size() << "]: \n";
          for (const auto& element : elements)
          {
            InternalToString(element, oss);
          }
        }
        break;
        default:
        {
          oss << "Array: unknown type.\n";
        }
        break;
      }
    }
    break;
  }
}

std::string ToString(const RedisMessage& msg)
{
  std::ostringstream oss;
  InternalToString(msg, oss);
  return oss.str();
}
