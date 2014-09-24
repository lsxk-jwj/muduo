#ifndef MUDUO_EXAMPLES_HIREDIS_HIREDIS_H
#define MUDUO_EXAMPLES_HIREDIS_HIREDIS_H

#include <muduo/base/StringPiece.h>
#include <muduo/base/Types.h>
#include <muduo/net/Callbacks.h>
#include <muduo/net/InetAddress.h>

#include <boost/bind.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>

#include <hiredis/async.h>
#include <hiredis/hiredis.h>

//struct redisAsyncContext;
//struct redisReply;

namespace muduo
{
namespace net
{
class Channel;
class EventLoop;
}
}

namespace hiredis
{

// FIXME: Free context_ with ::redisAsyncFree
class Hiredis : public boost::enable_shared_from_this<Hiredis>,
                boost::noncopyable
{
 public:
  typedef boost::function<void(const redisAsyncContext*, int)> ConnectCallback;
  typedef boost::function<void(const redisAsyncContext*, int)> DisconnectCallback;
  typedef boost::function<void(redisAsyncContext*, redisReply*, void*)> CommandCallback;

  Hiredis(muduo::net::EventLoop* loop, const muduo::net::InetAddress& serverAddr);
  ~Hiredis();

  const muduo::net::InetAddress& serverAddress() const { return serverAddr_; }
  redisAsyncContext* context() { return context_; }

  void setConnectCallback(const ConnectCallback& cb) { connectCb_ = cb; }
  void setDisconnectCallback(const DisconnectCallback& cb) { disconnectCb_ = cb; }

  void connect();
  void disconnect();  // FIXME: implement this with redisAsyncDisconnect

  // regular command, WARNING: only allow one command at any time, do not use!!!
  int command(const CommandCallback& cb, muduo::StringArg cmd);
  // FIXME: make it usable for varargs.

  int ping();

 private:
  void handleRead(muduo::Timestamp receiveTime);
  void handleWrite();

  int fd() const;
  void logConnection(bool up) const;
  void setChannel();
  void removeChannel();

  void connectCallback(int status);
  void disconnectCallback(int status);

  static Hiredis* getHiredis(const redisAsyncContext* ac);

  static void connectCallback(const redisAsyncContext* ac, int status);
  static void disconnectCallback(const redisAsyncContext* ac, int status);
  // regular command callback
  static void commandCallback(redisAsyncContext* ac, void*, void*);

  static void addRead(void* privdata);
  static void delRead(void* privdata);
  static void addWrite(void* privdata);
  static void delWrite(void* privdata);
  static void cleanup(void* privdata);

  void pingCallback(redisAsyncContext* ac, redisReply* reply, void* privdata);

 private:
  muduo::net::EventLoop* loop_;
  const muduo::net::InetAddress serverAddr_;
  redisAsyncContext* context_;
  boost::shared_ptr<muduo::net::Channel> channel_;
  ConnectCallback connectCb_;
  DisconnectCallback disconnectCb_;
  CommandCallback commandCb_;
};

}

#endif  // MUDUO_EXAMPLES_HIREDIS_HIREDIS_H