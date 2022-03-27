// Copyright 2011, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#ifndef MUDUO_EXAMPLES_PROTOBUF_CODEC_DISPATCHER_H
#define MUDUO_EXAMPLES_PROTOBUF_CODEC_DISPATCHER_H

#include "muduo/base/noncopyable.h"
#include "muduo/net/Callbacks.h"

#include <google/protobuf/message.h>

#include <unordered_map>

#include <type_traits>

typedef std::shared_ptr<google::protobuf::Message> MessagePtr;

//这是一个抽象基类
class Callback : muduo::noncopyable
{
 public:
  virtual ~Callback() = default;

  //对应着不同的message，onMessage设计为虚函数
  virtual void onMessage(const muduo::net::TcpConnectionPtr&,
                         const MessagePtr& message,
                         muduo::Timestamp) const = 0;
};

// 分发器的一种常见的设计手法，定义 模板派生类 去继承 抽象基类
// 使用onMessage虚函数是因为：不同的派生类对象拥有着不同的message，于是对应着不同的ProtoMessageCallback！
// 使用模板参数T是因为：通过模板推倒可以得到具体消息的类型即T，从而解决dynamic_cast<T>中T未知的问题！
template <typename T>
class CallbackT : public Callback
{
  static_assert(std::is_base_of<google::protobuf::Message, T>::value,
                "T must be derived from gpb::Message.");
 public:
  typedef std::function<void (const muduo::net::TcpConnectionPtr&,
                                const std::shared_ptr<T>& message,
                                muduo::Timestamp)> ProtobufMessageTCallback;

  CallbackT(const ProtobufMessageTCallback& callback)
    : callback_(callback)
  {
  }
  
  //真正的对应着特定message的特定callback！
  void onMessage(const muduo::net::TcpConnectionPtr& conn,
                 const MessagePtr& message,
                 muduo::Timestamp receiveTime) const override
  {

    std::shared_ptr<T> concrete = muduo::down_pointer_cast<T>(message);
    assert(concrete != nullptr);
    callback_(conn, concrete, receiveTime);
  }

 private:
  ProtobufMessageTCallback callback_;//回调函数真正保存的位置！
};

//分配器的第二种设计方案: 根据不同的message类型区调用不同的回调函数（消息处理函数）！

class ProtobufDispatcher
{
 public:
  typedef std::function<void (const muduo::net::TcpConnectionPtr&,
                                const MessagePtr& message,
                                muduo::Timestamp)> ProtobufMessageCallback;

  explicit ProtobufDispatcher(const ProtobufMessageCallback& defaultCb)
    : defaultCallback_(defaultCb)
  {
  }

  void onProtobufMessage(const muduo::net::TcpConnectionPtr& conn,
                         const MessagePtr& message,
                         muduo::Timestamp receiveTime) const
  {
    auto it = callbacks_.find(message->GetDescriptor());
    if (it != callbacks_.end())
    {
      it->second->onMessage(conn, message, receiveTime);
    }
    else
    {
      defaultCallback_(conn, message, receiveTime);
    }
  }
  
  //分配器含有一个模板函数，将用户自定义的不同类型的callback保存到映射表callbacks_
  template<typename T>
  void registerMessageCallback(const typename CallbackT<T>::ProtobufMessageTCallback& callback)
  {
    std::shared_ptr<CallbackT<T> > pd(new CallbackT<T>(callback));
    callbacks_[T::descriptor()] = pd;
  }

 private:
  //这个映射表保存的是 基类指针 -> 基类指针 的映射！
  typedef std::unordered_map<const google::protobuf::Descriptor*, std::shared_ptr<Callback> > CallbackMap;

  CallbackMap callbacks_; // 用户传进来的回调函数
  ProtobufMessageCallback defaultCallback_;// 默认的回调函数
};
#endif  // MUDUO_EXAMPLES_PROTOBUF_CODEC_DISPATCHER_H

