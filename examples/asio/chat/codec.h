#ifndef MUDUO_EXAMPLES_ASIO_CHAT_CODEC_H
#define MUDUO_EXAMPLES_ASIO_CHAT_CODEC_H

#include "muduo/base/Logging.h"
#include "muduo/net/Buffer.h"
#include "muduo/net/Endian.h"
#include "muduo/net/TcpConnection.h"

// 应用层解决tcp粘包是必须的，即使已经使用了缓冲区buffer，也还是要在应用层处理粘包的问题！
// 这个代码很简洁，是个好例子

class LengthHeaderCodec : muduo::noncopyable
{
 public:
  typedef std::function<void (const muduo::net::TcpConnectionPtr&,
                                const muduo::string& message,
                                muduo::Timestamp)> StringMessageCallback;

  explicit LengthHeaderCodec(const StringMessageCallback& cb)
    : messageCallback_(cb)// 给成员变量赋值就是向LengthHeaderCodec里注册了回调函数cb,这里是在
  {
  }

  // 收到数据之后要进行拆包，残包的处理方法也在这里！
  void onMessage(const muduo::net::TcpConnectionPtr& conn,
                 muduo::net::Buffer* buf,
                 muduo::Timestamp receiveTime)
  {
    while (buf->readableBytes() >= kHeaderLen) // kHeaderLen == 4
    {
      // FIXME: use Buffer::peekInt32()
      const void* data = buf->peek();

      //从第一个字节开始读取，将data（指向char的指针）转化为be（指向int_32t的指针）
      //解引用时，int32_t的指针将会读取首字节往后四个字节的内容将其转换为一个 int32_t型的变量！
      int32_t be32 = *static_cast<const int32_t*>(data); // SIGBUS
      const int32_t len = muduo::net::sockets::networkToHost32(be32);
      if (len > 65536 || len < 0)
      {
        LOG_ERROR << "Invalid length " << len;
        conn->shutdown();  // FIXME: disable reading
        break;
      }
      else if (buf->readableBytes() >= len + kHeaderLen) //这是一个完整包，先读出长度，再读出实际内容，再调用回调函数！
      {
        buf->retrieve(kHeaderLen);
        muduo::string message(buf->peek(), len);
        messageCallback_(conn, message, receiveTime);
        buf->retrieve(len);
      }
      else // 表明是一个残包，那这次函数调用就什么也不处理，buf里会保存这个残包的，也是非阻塞式网路编程必然要用到缓冲区buffer的原因之一！
      {
        break;
      }
    }
  }

  // FIXME: TcpConnectionPtr
  // 将数据打包好
  void send(muduo::net::TcpConnection* conn,
            const muduo::StringPiece& message)
  {
    muduo::net::Buffer buf;
    buf.append(message.data(), message.size());//数据本身部分！
    int32_t len = static_cast<int32_t>(message.size());
    int32_t be32 = muduo::net::sockets::hostToNetwork32(len);
    buf.prepend(&be32, sizeof be32);//将数据的长度添加到头部！
    conn->send(&buf);
  }

 private:
  StringMessageCallback messageCallback_;
  const static size_t kHeaderLen = sizeof(int32_t);
};

#endif  // MUDUO_EXAMPLES_ASIO_CHAT_CODEC_H
