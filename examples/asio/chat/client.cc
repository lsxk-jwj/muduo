#include "examples/asio/chat/codec.h"

#include "muduo/base/Logging.h"
#include "muduo/base/Mutex.h"
#include "muduo/net/EventLoopThread.h"
#include "muduo/net/TcpClient.h"

#include <iostream>
#include <stdio.h>
#include <unistd.h>

using namespace muduo;
using namespace muduo::net;

class ChatClient : noncopyable
{
 public:
  ChatClient(EventLoop* loop, const InetAddress& serverAddr)
    : client_(loop, serverAddr, "ChatClient"),
      codec_(std::bind(&ChatClient::onStringMessage, this, _1, _2, _3))//仍然注册回调
  {
    client_.setConnectionCallback(
        std::bind(&ChatClient::onConnection, this, _1));
    client_.setMessageCallback(
        std::bind(&LengthHeaderCodec::onMessage, &codec_, _1, _2, _3));
    client_.enableRetry();
  }

  void connect()
  {
    client_.connect();
  }

  void disconnect()
  {
    client_.disconnect();
  }

  // 由main线程调用
  void write(const StringPiece& message)
  {
    MutexLockGuard lock(mutex_);
    if (connection_)
    {
      codec_.send(get_pointer(connection_), message);
    }
  }

 private:
  // 由eventLoop线程调用
  void onConnection(const TcpConnectionPtr& conn)
  {
    LOG_INFO << conn->localAddress().toIpPort() << " -> "
             << conn->peerAddress().toIpPort() << " is "
             << (conn->connected() ? "UP" : "DOWN");

    MutexLockGuard lock(mutex_);
    if (conn->connected())
    {
      connection_ = conn;
    }
    else
    {
      connection_.reset();
    }
  }

  void onStringMessage(const TcpConnectionPtr&,
                       const string& message,
                       Timestamp)
  {
    printf("<<< %s\n", message.c_str());
  }

  TcpClient client_;
  LengthHeaderCodec codec_;
  MutexLock mutex_;
  TcpConnectionPtr connection_ GUARDED_BY(mutex_); //当网络库建立好连接时，会返回conn对象，保存在connection_中！
};

int main(int argc, char* argv[])
{
  LOG_INFO << "pid = " << getpid();
  if (argc > 2)
  {
    EventLoopThread loopThread;
    uint16_t port = static_cast<uint16_t>(atoi(argv[2]));
    InetAddress serverAddr(argv[1], port);

    ChatClient client(loopThread.startLoop(), serverAddr);
    client.connect();
    std::string line;
    while (std::getline(std::cin, line))
    {
      client.write(line);
    }
    client.disconnect();
    CurrentThread::sleepUsec(1000*1000);  // wait for disconnect, see ace/logging/client.cc
  }
  else
  {
    printf("Usage: %s host_ip port\n", argv[0]);
  }
}

