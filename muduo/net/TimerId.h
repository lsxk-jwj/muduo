// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//
// This is a public header file, it must only include public header files.

#ifndef MUDUO_NET_TIMERID_H
#define MUDUO_NET_TIMERID_H

#include "muduo/base/copyable.h"

namespace muduo
{
namespace net
{

class Timer;

///
/// An opaque identifier, for canceling Timer.
///
class TimerId : public muduo::copyable
{
 public:
  TimerId()
    : timer_(nullptr),
      sequence_(0)
  {
  }

  TimerId(std::shared_ptr<Timer> timer, int64_t seq)
    : timer_(timer),
      sequence_(seq)
  {
  }

  // default copy-ctor, dtor and assignment are okay

  TimerId(const TimerId& ) = default;
  ~TimerId() = default;
  TimerId& operator= (const TimerId& ) = default; 

  friend class TimerQueue;

 private:
  std::shared_ptr<Timer> timer_;
  int64_t sequence_;
};

}  // namespace net
}  // namespace muduo

#endif  // MUDUO_NET_TIMERID_H
