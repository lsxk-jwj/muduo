#include "examples/protobuf/codec/codec.h"
#include "examples/protobuf/codec/dispatcher.h"
#include "examples/protobuf/codec/query.pb.h"

#include "muduo/base/Logging.h"
#include "muduo/base/Mutex.h"
#include "muduo/net/EventLoop.h"
#include "muduo/net/TcpServer.h"

#include <stdio.h>
#include <unistd.h>

using namespace muduo;
using namespace muduo::net;

//Query Answer都是protobuf::message的派生类!
typedef std::shared_ptr<muduo::Query> QueryPtr;
typedef std::shared_ptr<muduo::Answer> AnswerPtr;

class QueryServer : noncopyable
{
 public:

  // 这几个类之间的耦合都是通过回调函数实现的！
  // 不同的成员去注册各自的回调函数，就是层层回调函数这个设计非常绕，需要理清楚！
  QueryServer(EventLoop* loop,
              const InetAddress& listenAddr)
  : server_(loop, listenAddr, "QueryServer"),
    dispatcher_(std::bind(&QueryServer::onUnknownMessage, this, _1, _2, _3)),
    codec_(std::bind(&ProtobufDispatcher::onProtobufMessage, &dispatcher_, _1, _2, _3))
  {
    dispatcher_.registerMessageCallback<muduo::Query>(
        std::bind(&QueryServer::onQuery, this, _1, _2, _3));

    dispatcher_.registerMessageCallback<muduo::Answer>(
        std::bind(&QueryServer::onAnswer, this, _1, _2, _3));

    server_.setConnectionCallback(
        std::bind(&QueryServer::onConnection, this, _1));

    server_.setMessageCallback(
        std::bind(&ProtobufCodec::onMessage, &codec_, _1, _2, _3));
  }

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
  }

  void onUnknownMessage(const TcpConnectionPtr& conn,
                        const MessagePtr& message,
                        Timestamp)
  {
    LOG_INFO << "onUnknownMessage: " << message->GetTypeName();
    conn->shutdown();
  }

  //query消息处理函数
  void onQuery(const muduo::net::TcpConnectionPtr& conn,
               const QueryPtr& message,
               muduo::Timestamp)
  {
    LOG_INFO << "onQuery:\n" << message->GetTypeName() << message->DebugString();
    Answer answer;
    answer.set_id(1);
    answer.set_questioner("jing weijie");
    answer.set_answerer("blog.csdn.net/Solstice");
    answer.add_solution("Jump!");
    answer.add_solution("Win!");
    codec_.send(conn, answer);//使用codec_可以直接发送answer，codec_负责将其打包成为buffer。

    conn->shutdown();
  }

  //answer消息处理函数
  void onAnswer(const muduo::net::TcpConnectionPtr& conn,
                const AnswerPtr& message,
                muduo::Timestamp)
  {
    LOG_INFO << "onAnswer: " << message->GetTypeName();
    conn->shutdown();
  }

  TcpServer server_;
  ProtobufDispatcher dispatcher_;
  ProtobufCodec codec_;
};

int main(int argc, char* argv[])
{
  LOG_INFO << "pid = " << getpid();
  if (argc > 1)
  {
    //eventloop对象不必是堆上对象，它的生命周期和线程一样长，创建eventloop对象的main线程就是IO线程！
    EventLoop loop;
    uint16_t port = static_cast<uint16_t>(atoi(argv[1]));
    InetAddress serverAddr(port);
    QueryServer server(&loop, serverAddr);
    server.start();
    loop.loop();
  }
  else
  {
    printf("Usage: %s port\n", argv[0]);
  }
}

