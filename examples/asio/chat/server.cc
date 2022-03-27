#include "examples/asio/chat/codec.h"

#include "muduo/base/Logging.h"
#include "muduo/base/Mutex.h"
#include "muduo/net/EventLoop.h"
#include "muduo/net/TcpServer.h"

#include <unordered_set>
#include <stdio.h>
#include <unistd.h>

using namespace muduo;
using namespace muduo::net;

//非阻塞式的广播聊天服务器！！！！！
class ChatServer : noncopyable
{
 public:
  ChatServer(EventLoop* loop,
             const InetAddress& listenAddr)
  : server_(loop, listenAddr, "ChatServer"),
    codec_(std::bind(&ChatServer::onStringMessage, this, _1, _2, _3))//将onStringMessage回调函数注册给codec
  {
    server_.setConnectionCallback(
        std::bind(&ChatServer::onConnection, this, _1));

    //LengthHeaderCodec 相当于一个中间层
    server_.setMessageCallback(
        std::bind(&LengthHeaderCodec::onMessage, &codec_, _1, _2, _3));
  }

  //start 是不能放在构造函数里面调用的，否则会有线程安全的问题！
  void start()
  {
    server_.start();
  }

 private:
  void onConnection(const TcpConnectionPtr& conn)
  {
    LOG_INFO << conn->peerAddress().toIpPort() << " -> "
             << conn->localAddress().toIpPort() << " is "
             << (conn->connected() ? "UP" : "DOWN");

    if (conn->connected())
    {
      connections_.emplace(conn);
    }
    else
    {
      connections_.erase(conn);
    }
  }

  //这就是用户自定义的回调函数，它必须符合约定好的StringMessageCallback类型！
  void onStringMessage(const TcpConnectionPtr&,
                       const string& message,
                       Timestamp)
  {
    // 对所有用户发送消息，即广播，且是非阻塞的广播！
    // 表明网络库设计好了，自定义的server代码实现其实很简单！
    for (auto it = connections_.begin();
        it != connections_.end();
        ++it)
    {
      codec_.send(get_pointer(*it), message);
    }
  }

  typedef std::unordered_set<TcpConnectionPtr> ConnectionList;
  TcpServer server_;
  LengthHeaderCodec codec_;// 将编解码器作为Chatserver的成员，将LengthHeaderCodec::onMessage注册给server_,
  ConnectionList connections_;
};

int main(int argc, char* argv[])
{
  LOG_INFO << "pid = " << getpid();
  if (argc > 1)
  {
    EventLoop loop;
    uint16_t port = static_cast<uint16_t>(atoi(argv[1]));
    InetAddress serverAddr(port);
    ChatServer server(&loop, serverAddr);
    server.start();
    loop.loop();
  }
  else
  {
    printf("Usage: %s port\n", argv[0]);
  }
}

