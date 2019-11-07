#include "redis_client.h"
#include <charconv>

#define SIMPLE_STR_PREFIX '+'
#define ERROR_PREFIX '-'
#define INTEGER_PREFIX ':'
#define BULK_STR_PREFIX '$'
#define ARRAY_PREFIX '*'

#define RCE_SUCCESS 0
#define RCE_LESSDATA 1
#define RCE_PROTOCOL 2

RedisClient::RedisClient(App& app, const ConnectedCallback& cb_conn,
  const DisconnectCallback& cb_disconn)
  : ioctx_(app.IoCtx())
  , connected_callback_(cb_conn)
  , disconnect_callback_(cb_disconn)
{
  session_ = std::make_shared<RedisClient::Session>(
    this, connected_callback_, disconnect_callback_);
}

void RedisClient::Connect(asio::ip::tcp::endpoint server)
{
  session_->Create(server);
}

void RedisClient::Close()
{
  session_->Close();
}

void RedisClient::Command(std::string_view cmd, const CommandCallback& cb_cmd)
{
  cmds_.push_back({{cmd.data(), cmd.size()}, cb_cmd});
  if (cmds_.size() == 1)
  {
    const auto& closure = cmds_.front();
    session_->Write(closure.cmd);
  }
}

void RedisClient::OnCommandComplete()
{
  if (cmds_.empty())
  {
    return;
  }

  cmds_.pop_front();

  if (!cmds_.empty())
  {
    const auto& closure = cmds_.front();
    session_->Write(closure.cmd);
  }
}

int RedisClient::Parse(std::string_view sv)
{
  auto ret = InternalDecode(sv);
  if (ret)
  {
    if (!cmds_.empty())
    {
      const auto& cmd = cmds_.front();
      cmd.callback.Invoke(ret);
    }

    return RCE_SUCCESS;
  }

  return ret.Error();
}

ZResult<RedisMessage> RedisClient::InternalDecode(std::string_view& sv)
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

ZResult<RedisString> RedisClient::DecodeSimpleStr(std::string_view& sv)
{
  auto pos = sv.find_first_of("\r\n");
  if (pos == std::string_view::npos)
  {
    return Failure(RCE_LESSDATA);
  }

  RedisString ret = std::string{sv.substr(1, pos - 1)};
  sv.remove_prefix(pos + 2);
  return ret;
}

ZResult<RedisError> RedisClient::DecodeError(std::string_view& sv)
{
  auto pos = sv.find_first_of("\r\n");
  if (pos == std::string_view::npos)
  {
    return Failure(RCE_LESSDATA);
  }

  RedisError ret{sv.substr(1, pos - 1)};
  sv.remove_prefix(pos + 2);
  return ret;
}

ZResult<int64_t> RedisClient::DecodeInteger(std::string_view& sv)
{
  auto pos = sv.find_first_of("\r\n");
  if (pos == std::string_view::npos)
  {
    return Failure(RCE_LESSDATA);
  }

  int64_t ret;
  auto [ptr, ec] = std::from_chars(sv.data() + 1, sv.data() + pos, ret);
  if (ec != std::errc{})
  {
    return Failure(RCE_PROTOCOL);
  }

  sv.remove_prefix(pos + 2);
  return Success(ret);
}

ZResult<RedisString> RedisClient::DecodeBulkStr(std::string_view& sv)
{
  std::string_view sv_copy(sv);
  auto pos = sv_copy.find_first_of("\r\n");
  if (pos == std::string_view::npos)
  {
    return Failure(RCE_LESSDATA);
  }

  int len = 0;
  auto [ptr, ec] =
    std::from_chars(sv_copy.data() + 1, sv_copy.data() + pos, len);
  if (ec != std::errc{} || len < -1)
  {
    return Failure(RCE_PROTOCOL);
  }

  if (len == -1)
  {
    // null string
    return RedisString(nullptr);
  }

  if (sv_copy.size() < pos + 2 + len + 2)
  {
    return Failure(RCE_LESSDATA);
  }

  sv.remove_prefix(pos + 2 + len + 2);
  return std::string(sv_copy.data() + pos + 2, len);
}

ZResult<RedisArray> RedisClient::DecodeArray(std::string_view& sv)
{
  std::string_view sv_copy(sv);
  auto pos = sv_copy.find_first_of("\r\n");
  if (pos == std::string_view::npos)
  {
    return Failure(RCE_LESSDATA);
  }

  int len = 0;
  auto [ptr, ec] =
    std::from_chars(sv_copy.data() + 1, sv_copy.data() + pos, len);
  if (ec != std::errc{} || len < -1)
  {
    return Failure(RCE_PROTOCOL);
  }

  if (len == -1)
  {
    // null array
    return RedisArray{nullptr};
  }

  sv.remove_prefix(pos + 2);

  RedisArray ret{RedisArray::Elements(len)};
  for (int i = 0; i < len; ++i)
  {
    ZResult<RedisArray::Element> result = InternalDecode(sv);
    if (!result)
    {
      return Failure(result.Error());
    }

    std::get<1>(ret.array)[i] = result.Value();
  }

  return ret;
}

//////////////////////////////////////////////////////////////////////////////
// RedisClient::Session
RedisClient::Session::Session(RedisClient* client,
  const ConnectedCallback& cb_conn, const DisconnectCallback& cb_disconn)
  : client_(client)
  , socket_(client->ioctx_)
  , conn_callback_(cb_conn)
  , disconn_callback_(cb_disconn)
{
}

void RedisClient::Session::Create(asio::ip::tcp::endpoint server)
{
  auto self = shared_from_this();
  socket_.async_connect(server, [self](const std::error_code& ec) {
    if (ec)
    {
      self->conn_callback_.Invoke(ec.value());
      return;
    }

    self->conn_callback_.Invoke(RCE_SUCCESS);

    self->Read();
  });
}

void RedisClient::Session::Close()
{
  if (!socket_.is_open())
  {
    return;
  }

  socket_.close();
}

void RedisClient::Session::Read()
{
  auto self = shared_from_this();
  socket_.async_read_some(asio::buffer(rbuffer_ + rpos_, RBUFSIZE - rpos_),
    [self](const std::error_code& ec, std::size_t len) {
      if (ec)
      {
        self->Close();
        self->disconn_callback_.Invoke();
        return;
      }

      self->rpos_ += len;

      char* buffer = self->rbuffer_;
      size_t end = self->rpos_;
      if (end > 2 && buffer[end - 2] == '\r' && buffer[end - 1] == '\n')
      {
        std::string_view sv(buffer, end);
        if (self->client_->Parse(sv) == RCE_SUCCESS)
        {
          // reset read position
          self->rpos_ = 0;
        }
      }

      self->client_->OnCommandComplete();
    });
}

void RedisClient::Session::Write(const std::string& cmd)
{
  auto self = shared_from_this();
  asio::async_write(socket_, asio::buffer(cmd.data(), cmd.size()),
    [self](const std::error_code& ec, std::size_t len) {
      if (ec)
      {
        self->Close();
        self->disconn_callback_.Invoke();
        return;
      }
    });
}
