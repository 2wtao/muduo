#pragma once

#include"noncopyable.h"
#include"Timestamp.h"

#include<functional>
#include<memory>

class EventLoop;

/*
 * Channel 理解为通道，封装了sockfd和其感兴趣的event，如EPOLLIN、EPOLLOUT事件
 * 还绑定了poller返回的具体事件
*/

class Channel : noncopyable
{
public:
   using EventCallback = std::function<void()>;
   using ReadEventCallback = std::function<void(Timestamp)>;

   Channel(EventLoop *loop, int fd);
   ~Channel();
   
   //fd得到poller通知以后，处理事件的
   void handleEvent(Timestamp receiveTime);

   //设置回调函数对象
   void setReadCallback(ReadEventCallback cb) 
   { 
       readCallback_ = std::move(cb);
   }
   void setWriteCallback(EventCallback cb)
   {
       writeCallback_ = std::move(cb);
   }
   void setCloseCallback(EventCallback cb)
   {
       closeCallback_ = std::move(cb);
   }
   void setErrorCallback(EventCallback cb)
   {
       errorCallback_ = std::move(cb);
   }

   //防止channel被手动remove掉，channel还在执行回调操作
   void tie(const std::shared_ptr<void>&);

   int fd() const  { return fd_; }
   int evnets() const { return events_; }
   void set_revents(int revt) { revents_ = revt; }
   
   //设置fd相应的事件状态
   void enableReading()  
   { 
       events_ |= kReadEvent;
       update(); 
   }
   void disableReading()  
   { 
       events_ &= ~kReadEvent;
       update(); 
   }
   void enableWriteing()  
   { 
       events_ |= kWriteEvent;
       update(); 
   }
   void disableWriteing()  
   { 
       events_ |= kWriteEvent;
       update(); 
   }
   void disableAll()  
   { 
       events_ |= kNoneEvent;
       update(); 
   }

   //返回fd当前的事件状态
   bool isNoneEvent() const { return events_ == kNoneEvent; }
   bool isWriting() const { return events_ & kWriteEvent; }
   bool isReading() const { return events_ & kReadEvent; }

   int index() { return index_; }
   void set_index(int idx) { index_ = idx; }


   //one loop per thread      //duo  channel  per loop
   EventLoop* ownerLoop() { return loop_;}


   void update();
   void remove();
   void handleEventWithGuard(Timestamp receiveTime);  //handle(处理) Event(事件) WithGuard(受保护的)


private:
   static const int kNoneEvent;
   static const int kReadEvent;
   static const int kWriteEvent;

   EventLoop *loop_;  //事件循环（所属的事件循环）
   const int fd_;   //fd，就是poller监听的对象
   int events_;  //注册fd感兴趣的事件
   int revents_;  //发生的事件
   int index_;  //**表示的就是channel的状态（未添加、已添加和已删除）
   //新创建的channel默认值为-1，通过updatechannel和removechannel的一个操作改变其对应的值

   std::weak_ptr<void> tie_;
   bool tied_;

   //因为Channel通道里边能够获知fd最终发生的具体的事件revent，所以它负责调用具体事件的回调操作；
   ReadEventCallback readCallback_;
   EventCallback writeCallback_;
   EventCallback closeCallback_;
   EventCallback errorCallback_;

};