#pragma once

/**
 * 
 */

#include "EventLoop.h"
#include "Acceptor.h"
#include "InetAddress.h"
#include "noncopyable.h"
#include "EventLoopThreadPool.h"
#include "Callbacks.h"
#include "TcpConnection.h"
#include "Buffer.h"

#include <functional>
#include <string>
#include <memory>
#include <unordered_map>

//对外的服务器编程使用的类
class TcpServer:noncopyable
{
public:
    using ThreadInitCallback = std::function<void(EventLoop*)>;

    //端口号的一个重用与否
    enum Option
    {
        kNoReusePort,
        kReusePort,
    };

    TcpServer(EventLoop *loop,     
              const InetAddress &listenAddr,
              const std::string &nameArg,
              Option option = kNoReusePort);
    ~TcpServer();

    void setThreadInitcallback(const ThreadInitCallback &cb){ threadInitCallback_ = cb; }  //线程初始化回调函数
    void setConnectionCallback(const ConnectionCallback &cb){ connectionCallback_ = cb; }  //链接事件灰调函数
    void setMessageCallback(const MessageCallback &cb) { messageCallback_ = cb; }//读写事件回调函数
    void setWriteCompleteCallback(const WriteCompleteCallback &cb) { writeCompleteCallback_ = cb; }

    void setThreadNum(int numThreads);  //设置底层EventLoop的数量

    void start(); //开启服务监听
private:
    void newConnection(int sockfd, const InetAddress &peerAddr);  //增加新连接
    void removeConnection(const TcpConnectionPtr &conn);  //删除链接
    void removeConnectionInloop(const TcpConnectionPtr &conn); //在一个loop中删除该链接
 
    using ConnectionMap = std::unordered_map<std::string, TcpConnectionPtr>; ///////////////////////连接的名字和对应的连接


    EventLoop *loop_;
    const std::string ipPort_;
    const std::string name_;

    std::unique_ptr<Acceptor> acceptor_;
    std::unique_ptr<EventLoopThreadPool> threadPool_;   //one loop per thread;

    ConnectionCallback connectionCallback_;  //有新链接的回调
    MessageCallback messageCallback_;      //有读写事件的回调
    WriteCompleteCallback writeCompleteCallback_; //消息发送完成以后的回调

    ThreadInitCallback threadInitCallback_; //loop线程初始化的回调

    std::atomic_int started_;

    int nextConnId_;
    ConnectionMap connections_; //保存所有的链接
};