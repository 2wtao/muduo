#include "EventLoop.h"
#include "Logger.h"
#include "Poller.h"
#include "Channel.h"

#include <sys/eventfd.h>   //注意这个头文件
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

//防止一个线程创建多个EventLoop
__thread EventLoop *t_loopInThisThread = nullptr;   //__thread 是一个thread_local的机制
//one loop per thread

//默认的Poller IO复用接口的超时时间
const int kPollTimeMs = 1000;

//创建wakeupfd，用来notify唤醒subReactor处理新来的channel （轮偱的方式）
int createEventfd()
{
    int evtfd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if(evtfd < 0)
    {
      LOG_FATAL("eventfd error:%d", errno);
    }
    return evtfd;
}

EventLoop::EventLoop()
   : looping_(false)
   , quit_(false)
   , callingPendingFunctors_(false)    //表示当前loop是否需要执行的回调操作
   , threadId_(CurrentThread::tid())
   , poller_(Poller::newDefaultPoller(this))
   , wakeupFd_(createEventfd())
   , wakeupChannel_(new Channel(this, wakeupFd_))
   , currentActiveChannel_(nullptr)
{
    LOG_DEBUG("EventLoop created %p in thread %d \n",this,threadId_);
    if(t_loopInThisThread)  //这个线程的loop
    {
        LOG_FATAL("Another EventLoop %p exists in this thread %d \n",t_loopInThisThread, threadId_);
    }
    else{
        t_loopInThisThread = this;
    }

    //设置wakeupfd放入事件类型以及发生事件后的回调操作
    wakeupChannel_->setReadCallback(std::bind(&EventLoop::handleRead,this));
    //每一个eventloop都监听wakechannel的EPOLLIN读事件了
    wakeupChannel_->enableReading();
}
EventLoop::~EventLoop()
{
    wakeupChannel_->disableAll();   //析构时对所有事件都不感兴趣了
    wakeupChannel_->remove();
    close(wakeupFd_);
    t_loopInThisThread = nullptr;
}


//开启事件循环
void EventLoop::loop()
{
    looping_ = true;
    quit_ = false;

    LOG_INFO("EventLoop %p start looping \n",this);

    while(!quit_)
    {
       //每次循环都将activeChannels_里边清除干净；
       activeChannels_.clear();  
       
       //poll执行后返回一个“now”
       //监听两类fd  一、client的fd；二、wakeupfd
       pollReturnTime_ = poller_->poll(kPollTimeMs,&activeChannels_);  //超时时间和 返回发生事件的vector
       for(auto&channel: activeChannels_)  //此时一轮poll已经完成，返回activeChannels_;
       {
          //Poller监听哪些cahnnel发生事件了，然后上报给EvnetLoop，通知channel处理相应的事件
          channel->handleEvent(pollReturnTime_);
          //
       }
       //执行当前EventLoop事件循环需要处理的回调操作;
       /**
        * IO线程的mainLoop主要负责新用户的accept  返回的fd 包装成=>channel 调用subloop进行fd上的读写事件
        * mainLoop事先注册一个回调cb（需要subloop来执行）  wakeupfd唤醒subloop执行pendingFunctors里边之前mainLoop注册的cb回调操作
        */
       doPendingFunctors();
    }

    LOG_INFO("EventLoop %p stop looping \n",this);
    looping_ = false;
} 

//退出事件循环 1.loop在自己的线程中调用quit  2.在非loop的线程中，调用loop的quit，先将其唤醒（有可能正阻塞）
/**
 *                mainloop
 *                                       no ============ 生产者-消费者的安全队列
 *   subLoop1     subLoop2     subLoop3
 *
 * 通过wakeupfd封装的eventfd直接进行唤醒的(以轮训的方式)
 */


//完事quit_都变成false,进不去while循环；
void EventLoop::quit()
{
    quit_ = true;

    if(!isInLoopThread())  //一个字：绝！//IO线程和worker线程之间没有用队列，直接用轮训的方式进行派发的；
    {
       wakeup();
    }
}      


//在当前loop中执行
void EventLoop::runInloop(Functor cb)
{
    if (isInLoopThread()) //在当前loop线程中执行cb(callback) 
    {
       cb();
    }
    else  //在非当前loop线程中执行cb，那就需要唤醒loop所在线程，执行cb
    {
       queueInLoop(cb);
    }

}

//把cb放到队列中，唤醒loop所在的线程，执行cb  //cb对应的channel不在你这个loop中
void EventLoop::queueInLoop(Functor cb)
{
    std::unique_lock<std::mutex> lock(mutex_);
    pendingFunctors_.emplace_back(cb);

    //唤醒相应的，需要执行上面回调操作的loop的线程
    //||的意思：当前loop正在执行回调，但又有了新的回调
    if(!isInLoopThread() || callingPendingFunctors_)
    {
       wakeup();
    }
}    


void EventLoop::handleRead()
{
    uint64_t one = 1;
    ssize_t n = read(wakeupFd_, &one, sizeof one);
    if(n != sizeof one)
    {
        LOG_ERROR("EventLoop::HeadleRead() reads %d bytes instead of 8",n);
    }
}

void EventLoop::doPendingFunctors()  //执行回调
{
    std::vector<Functor> functors;
    callingPendingFunctors_ = true;

    {
        std::unique_lock<std::mutex> lock(mutex_);
        functors.swap(pendingFunctors_);
    }

    for(const Functor &functor:functors)
    {
       functor();
    }

    callingPendingFunctors_ = false;
}


//唤醒loop所在的线程,//向wakeupfd_写一个数据,wakeupChannel就发生读事件，当前loop线程就会被唤醒
void EventLoop::wakeup()  
{
   uint64_t one = 1;
   ssize_t n = write(wakeupFd_, &one, sizeof one);
   if (n != sizeof one)
   {
      LOG_ERROR("EventLoop::wakeup() write %lu bytes instead of 8 \n",n);
   }
}

//EventLoop的方法，调用的是Poller的方 法
void EventLoop::updateChannel(Channel *channel)
{
    poller_->updateChannel(channel);
}

void EventLoop::removeChannel(Channel *channel)
{
    poller_->updateChannel(channel);
}

bool EventLoop::hasChannel(Channel *channel)
{
    return poller_->hasChannel(channel);
}