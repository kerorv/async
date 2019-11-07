#include <deque>
#include <memory>
#include <string>
#include <string_view>
#include "app.h"
#include "callback.h"
#include "resp_codec.h"
#include "result.h"
#include "thirtyparty/asio/asio.hpp"

using ConnectedCallback = Callback<void(int)>;
using DisconnectCallback = Callback<void()>;
using CommandCallback = Callback<void(const ZResult<RedisMessage>&)>;

class RedisClient
{
public:
  RedisClient(App& app, const ConnectedCallback& cb_conn,
    const DisconnectCallback& cb_disconn);

  void Connect(asio::ip::tcp::endpoint server);
  void Close();

  void Command(std::string_view cmd, const CommandCallback& cb_cmd);

private:
  struct Session : public std::enable_shared_from_this<Session>
  {
    Session(RedisClient* client, const ConnectedCallback& cb_conn,
      const DisconnectCallback& cb_disconn);
    void Create(asio::ip::tcp::endpoint server);
    void Close();
    void Read();
    void Write(const std::string& cmd);

    asio::ip::tcp::socket socket_;

    enum
    {
      RBUFSIZE = 64 * 1024
    };
    char rbuffer_[RBUFSIZE];
    size_t rpos_;
    RedisClient* client_;
    const ConnectedCallback& conn_callback_;
    const DisconnectCallback& disconn_callback_;
  };

  struct CommandClosure
  {
    std::string cmd;
    Callback<void(const ZResult<RedisMessage>&)> callback;
  };

  int Parse(std::string_view sv);
  void OnCommandComplete();

  ZResult<RedisMessage> InternalDecode(std::string_view& sv);
  ZResult<RedisString> DecodeSimpleStr(std::string_view& sv);
  ZResult<RedisError> DecodeError(std::string_view& sv);
  ZResult<int64_t> DecodeInteger(std::string_view& sv);
  ZResult<RedisString> DecodeBulkStr(std::string_view& sv);
  ZResult<RedisArray> DecodeArray(std::string_view& sv);

private:
  asio::io_context& ioctx_;
  std::deque<CommandClosure> cmds_;
  std::shared_ptr<Session> session_;
  ConnectedCallback connected_callback_;
  DisconnectCallback disconnect_callback_;
};
