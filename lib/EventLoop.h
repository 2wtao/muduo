/**(暂时理解，有更新随时更换)Eventloop扮演 Reactor反应堆角色，Channel和Poller扮演多路事件分发器角色
 * EventLoop用一个vector记录了所有的channel
 * Channel封装了sockfd和其感兴趣的事件，还有对应的回调操作，，，，Poller封装了epoll
 * 向channel注册sockfd对应感兴趣的事件，channel还封装了其对应的回调操作，
 * channel用EventLoop向Poller里边add/mod/del（update和remove）(等于epoll_ctl的操作)
 *                                  （Poller用的map表记录sockfd和其对应的cahnnel）
 * Poller里边epoll_wait第二个参数events用的也是一个vector(因为长度可变),默认初始值是16；
 * Poller向EventLoop返回发生的事件（返回的事件都记录在一个vector里边）
 * EventLoop调用channel封装的对调操作
 */


#pragma once

#include <functional>   //function 和 bind 
#include <vector>
#include <atomic>
#include <memory>
#include <mutex>

#include "noncopyable.h"
#include "Timestamp.h"
#include "CurrentThread.h"

class Channel;
class Poller;

//事件循环类  
//主要包含了两大模块  Channel(通道，负责fd，以及fd感兴趣的事件，以及感兴趣的事件发生后的一个返回)     Poller(epoll抽象)
class EventLoop:noncopyable
{
public:
    using Functor = std::function<void()>;
    
    EventLoop();
    ~EventLoop();

    //开启事件循环
    void loop();
    //退出事件循环
    void quit();
    
    Timestamp pollReturnTime() const { return pollReturnTime_; }

    //在当前loop中执行
    void runInloop(Functor cb);
    //把cb放到队列中，唤醒loop所在的线程，执行cb
    void queueInLoop(Functor cb);

    //唤醒loop所在的线程
    void wakeup();
    
    //EventLoop的方法，调用的是Poller的方 法
    void updateChannel(Channel *channel);
    void removeChannel(Channel *channel);
    bool hasChannel(Channel *channel);
    
    //判断EventLoop对象是否在自己的线程里面
    bool isInLoopThread() const { return threadId_ == CurrentThread::tid();}  //当前的线程号和创建looo的线程号
private: 
    void handleRead();  //wakeup
    void doPendingFunctors();  //执行回调

    using ChannelList = std::vector<Channel*>;   
    std::atomic_bool looping_;  //原子操作，底层通过CAS实现的
    std::atomic_bool quit_;  //标志退出loop循环

    const pid_t threadId_;  //记录当前loop所在线程id
    Timestamp pollReturnTime_;  //poller返回发生事件的channels的时间点
    std::unique_ptr<Poller> poller_;

    int wakeupFd_; //主要作用，当mainloop获取一个新用户的channel，通过轮训算法选择一个subloop，通过该成员唤醒subloop处理
    std::unique_ptr<Channel> wakeupChannel_;

    ChannelList activeChannels_;
    Channel *currentActiveChannel_;

    std::atomic_bool callingPendingFunctors_;  //标识当前loop是否需要执行的回调操作
    std::vector<Functor> pendingFunctors_;  //存储loop需要执行的所有的回调操作
    std::mutex mutex_; //互斥锁，用来保护上面vector容器的线程安全操作

};
















