#include"Channel.h"
#include"EventLoop.h"
#include"Logger.h"

#include<sys/epoll.h>

const int Channel::kNoneEvent = 0;
const int Channel::kReadEvent = EPOLLIN | EPOLLPRI;
const int Channel::kWriteEvent = EPOLLOUT;


//poller通知channel，你的fd上感兴趣的事件发生了，channel调用相应的回调操作
//线程和EventLoop(事件循环)一一对应，一个事件循环里边有一个poller和多个channel
 
// EventLoop: ChannelList+Poller (Poller监听的是，EventLoop里边保存的Channel)
Channel::Channel(EventLoop *loop, int fd)
    : loop_(loop)
    ,fd_(fd)
    ,events_(0)
    ,revents_(0)
    ,index_(-1)
    ,tied_(false)
     {}
Channel::~Channel() {}


//channel的tie方法什么时候调用过?   一个TcpConnection新连接创建的时候，  TcpConnection => Channel 
//就等于channel有一个弱智能指针，指向TcpConnection
void Channel::tie(const std::shared_ptr<void>& obj)
{
   tie_ = obj;        //tie_ 是弱智能指针
   tied_ = true;
}

/*
 *当改变channel所表示fd的events事件后，update负责poller里边更改fd相应的事件(epoll_ctl)
 */
void Channel::update()
{
   //通过channel所属的EventLoop，调用poller的相应方法，注册fd的events事件
   loop_->updateChannel(this);
}

//在channel所属的EventLoop中，把当前的channell删除掉
void Channel::remove()
{
   loop_->removeChannel(this);
}

void Channel::handleEvent(Timestamp receiveTime)
{
   if(tied_)
   {
      std::shared_ptr<void> guard = tie_.lock();
      if(guard)
      {
         handleEventWithGuard(receiveTime);
      }
   }
   else
   {
      handleEventWithGuard(receiveTime);
   }
}

//根据poller通知的channel发生的具体事件，由channel负责调用具体的回调操作
void Channel::handleEventWithGuard(Timestamp receiveTime)
{
   LOG_INFO("channel handleEvent revents:%d", revents_);

   if((revents_ & EPOLLHUP)  &&  !(revents_ & EPOLLIN))
   {
      if(closeCallback_)
      {
         closeCallback_();
      }
   }

   if(revents_ & EPOLLERR)  
   {
      if(errorCallback_)
      {
         errorCallback_();
      }
   }

   if(revents_ & (EPOLLIN | EPOLLPRI))
   {
      if(readCallback_)
      {
         readCallback_(receiveTime);
      }
   }

   if(revents_ & EPOLLOUT)
   {
      if(writeCallback_)
      {
         writeCallback_();
      }
   }
}

