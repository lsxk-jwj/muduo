#include "examples/hub/codec.h"

#include "muduo/base/Logging.h"
#include "muduo/net/EventLoop.h"
#include "muduo/net/TcpServer.h"

#include <map>
#include <set>
#include <stdio.h>

using namespace muduo;
using namespace muduo::net;

// 这是一个很好的分析需求 面向对象的开发任务学习示例！
// hub是一个单独的服务进程，如同一个消息队列一般；而发布者与订阅者可以做成同样的程序

namespace pubsub
{

typedef std::set<string> ConnectionSubscription;

class Topic : public muduo::copyable
{
 public:
  Topic(const string& topic)
    : topic_(topic)
  {
  }

  void add(const TcpConnectionPtr& conn)
  {
    audiences_.insert(conn);
    if (lastPubTime_.valid())
    {
      conn->send(makeMessage());
    }
  }

  void remove(const TcpConnectionPtr& conn)
  {
    audiences_.erase(conn);
  }

  // 广播应该是topic的成员
  void publish(const string& content, Timestamp time)
  {
    content_ = content;
    lastPubTime_ = time;
    string message = makeMessage();
    // 遍历容器来进行广播，非阻塞式的肯定也得遍历，但是这里的send函数是非阻塞的，交给网络库进行的数据收发！
    // 还可以进一步使用多线程来提高广播效率（不能使用全局锁，否则就会退化为单线程执行了）
    for (std::set<TcpConnectionPtr>::iterator it = audiences_.begin();
         it != audiences_.end();
         ++it)
    {
      (*it)->send(message);
    }
  }

 private:

  string makeMessage()
  {
    return "pub " + topic_ + "\r\n" + content_ + "\r\n";
  }

  string topic_;
  string content_;
  Timestamp lastPubTime_;
  std::set<TcpConnectionPtr> audiences_;//topic 自身保存订阅者的连接！
};

class PubSubServer : noncopyable
{
 public:
  PubSubServer(muduo::net::EventLoop* loop,
               const muduo::net::InetAddress& listenAddr)
    : loop_(loop),
      server_(loop, listenAddr, "PubSubServer")
  {
    server_.setConnectionCallback(
        std::bind(&PubSubServer::onConnection, this, _1));
    server_.setMessageCallback(
        std::bind(&PubSubServer::onMessage, this, _1, _2, _3));
    loop_->runEvery(1.0, std::bind(&PubSubServer::timePublish, this));
  }

  void start()
  {
    server_.start();
  }

 private:
  void onConnection(const TcpConnectionPtr& conn)
  {
    if (conn->connected())
    {
      conn->setContext(ConnectionSubscription());
    }
    else
    {
      const ConnectionSubscription& connSub
        = boost::any_cast<const ConnectionSubscription&>(conn->getContext());
      // subtle: doUnsubscribe will erase *it, so increase before calling.
      for (ConnectionSubscription::const_iterator it = connSub.begin();
           it != connSub.end();)
      {
        doUnsubscribe(conn, *it++);
      }
    }
  }

  void onMessage(const TcpConnectionPtr& conn,
                 Buffer* buf,
                 Timestamp receiveTime)
  {
    ParseResult result = kSuccess;

    //使用while是因为：一个客户（发布者和订阅者均可）可以一次发送了多条指令！
    while (result == kSuccess)
    {
      string cmd;
      string topic;
      string content;
      // 读取发布者或订阅者的数据并解析，这个函数就是一个简单的解析器（编解码器）。
      result = parseMessage(buf, &cmd, &topic, &content);
      if (result == kSuccess)
      {
        if (cmd == "pub")
        {
          doPublish(conn->name(), topic, content, receiveTime);
        }
        else if (cmd == "sub")
        {
          LOG_INFO << conn->name() << " subscribes " << topic;
          doSubscribe(conn, topic);
        }
        else if (cmd == "unsub")
        {
          doUnsubscribe(conn, topic);
        }
        else
        {
          conn->shutdown();
          result = kError;
        }
      }
      else if (result == kError)
      {
        conn->shutdown();
      }
    }
  }

  void timePublish()
  {
    Timestamp now = Timestamp::now();
    doPublish("internal", "utc_time", now.toFormattedString(), now);
  }

  // 订阅者conn 和其订阅的主题topic
  void doSubscribe(const TcpConnectionPtr& conn,
                   const string& topic)
  {
    ConnectionSubscription* connSub
      = boost::any_cast<ConnectionSubscription>(conn->getMutableContext());

    connSub->insert(topic);
    getTopic(topic).add(conn);
  }

  void doUnsubscribe(const TcpConnectionPtr& conn,
                     const string& topic)
  {
    LOG_INFO << conn->name() << " unsubscribes " << topic;
    getTopic(topic).remove(conn);
    // topic could be the one to be destroyed, so don't use it after erasing.
    ConnectionSubscription* connSub
      = boost::any_cast<ConnectionSubscription>(conn->getMutableContext());
    connSub->erase(topic);
  }

  void doPublish(const string& source,
                 const string& topic,
                 const string& content,
                 Timestamp time)
  {
    getTopic(topic).publish(content, time);
  }

  // 所以要保存一个映射表
  Topic& getTopic(const string& topic)
  {
    std::map<string, Topic>::iterator it = topics_.find(topic);//二分查找
    if (it == topics_.end())
    {
      it = topics_.insert(make_pair(topic, Topic(topic))).first;
    }
    return it->second;
  }

  EventLoop* loop_;
  TcpServer server_;
  std::map<string, Topic> topics_;//含有多个topic
};

}  // namespace pubsub

int main(int argc, char* argv[])
{
  if (argc > 1)
  {
    uint16_t port = static_cast<uint16_t>(atoi(argv[1]));
    EventLoop loop;
    if (argc > 2)
    {
      //int inspectPort = atoi(argv[2]);
    }
    pubsub::PubSubServer server(&loop, InetAddress(port));
    server.start();
    loop.loop();
  }
  else
  {
    printf("Usage: %s pubsub_port [inspect_port]\n", argv[0]);
  }
}

