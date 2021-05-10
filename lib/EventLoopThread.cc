#include "EventLoopThread.h"

#include "EventLoop.h"

EventLoopThread::EventLoopThread(const ThreadInitCallback &cb,
         const std::string &name)
         : loop_(nullptr)
         , exiting_(false)
         , thread_(std::bind(&EventLoopThread::threadFunc,this), name)
         , mutex_()
         , cond_()
         , callback_(cb)
{

}

EventLoopThread::~EventLoopThread()
{
  exiting_ = true;
  if(loop_ != nullptr)
  {
      loop_->quit();
      thread_.join();
  }
}
    
EventLoop* EventLoopThread::startLoop()
{
   thread_.start();  //loop开启时，底层的线程开启了，回调函数threadFunc()就开启了

   EventLoop *loop = nullptr;
   {
      std::unique_lock<std::mutex> lock(mutex_);      
      while( loop_ == nullptr )
      {
         cond_.wait(lock);
      }
      loop = loop_;
   } 
   return loop;
}

//此方法是在单独的线程中执行的
void EventLoopThread::threadFunc()
{
   EventLoop loop;  //创建一个独立的eventloop，和上面的线程一一对应

   if(callback_)
   {
       callback_(&loop);
   }

   {
       std::unique_lock<std::mutex> lock(mutex_);
       loop_ = &loop;
       cond_.notify_one();
   }

   loop.loop();  //loop会一直执行，到下一步说明loop已经结束循环了；

   std::unique_lock<std::mutex> lock(mutex_);
   loop_ = nullptr;
}