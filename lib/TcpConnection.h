#pragma once

#include "noncopyable.h"
#include "InetAddress.h"
#include "Callbacks.h"
#include "Buffer.h"
#include "Timestamp.h"

#include <memory>
#include <string>
#include <atomic>

class Channel;
class EventLoop;
class Socket;

/**用来打包成功链接服务器的客户端的这么一条通信链路
 * TcpServer => Acceptor => 有一个新用户链接，通过accept函数拿到connfd，
 * 
 * 打包TcpConnection 设置回调给 => 相应的Channel => Poller => Channel的回调操作
 */ 

class TcpConnection:noncopyable,public std::enable_shared_from_this<TcpConnection>
{
public:
    TcpConnection(EventLoop *loop
                 ,const std::string &nameArg
                 ,int Sockfd
                 ,const InetAddress& localAddr
                 ,const InetAddress& peerAddr);
    ~TcpConnection();
    
    EventLoop* getloop() const { return loop_; }
    const std::string& name() const { return name_; }
    const InetAddress& localAddress() const { return localAddr_; }
    const InetAddress& peerAddress() const { return peerAddr_; }

    bool connected() const { return state_==kConnected; }

    // 发送数据
    void send(const std::string &buf);
    // 关闭链接
    void shutdown();

    //回调操作
    void setConnectionCallback(const ConnectionCallback& cb)
    {
       connectionCallback_ = cb;
    }

    void setMessageCallback(const MessageCallback& cb)
    {
       messageCallback_ = cb;
    }

    void setWriteCompleteCallback(const WriteCompleteCallback& cb)
    {
       writeCompleteCallback_ = cb;
    }

    void setHighWaterMarkCallback(const HighWaterMarkCallback& cb, size_t highWaterMark)
    {
        highWaterMarkCallback_ = cb;
        highWaterMark_ = highWaterMark; 
    }

    void setCloseCallback(const CloseCallback& cb)
    {
        closeCallback_ = cb;
    }

    // 链接建立
    void connectEstablished();
    // 链接销毁
    void connectDestoryed();


private:
    enum StateE {kDisconnected //已经断开的
                ,kConnecting  //正在链接
                ,kConnected  //已经连接上了
                ,kDisconnecting};  //正在断开 
    void setState(StateE state) { state_ = state; }

    void handleRead(Timestamp receiveTime);
    void handleWrite();
    void handleClose();
    void handleError();

    void sendInLoop(const void* message, size_t len);

    void shutdownInLoop();

    EventLoop *loop_;  //这里的loop绝对不是baseloop，因为TcpConnection都是在subloop里边管理的
    const std::string name_;  //Tcpconnection的名字
    std::atomic_int state_;
    bool reading_;

                               //listenfd  ********                 connfd ***********
    // 这里和Acceptor类似  Acceptor ===> mainLoop        TcpConnection ===> subLoop
    std::unique_ptr<Socket> socket_;
    std::unique_ptr<Channel> channel_;

    const InetAddress localAddr_;  //主机的IP和端口号地址
    const InetAddress peerAddr_;  //客户端的，对端的

    ConnectionCallback connectionCallback_;  //有新链接的回调
    MessageCallback messageCallback_;      //有读写事件的回调
    WriteCompleteCallback writeCompleteCallback_; //消息发送完成以后的回调
    HighWaterMarkCallback highWaterMarkCallback_;
    CloseCallback closeCallback_;

    size_t highWaterMark_;  //水位线

    Buffer inputBuffer_; //读Buffer
    Buffer outputBuffer_; //写Buffer
};