#pragma once

#include "noncopyable.h"

#include <functional>
#include <string>
#include <vector>
#include <memory>

class EventLoop;
class EventLoopThread;

class EventLoopThreadPool:noncopyable
{
public:
    using ThreadInitCallback = std::function<void(EventLoop*)>;

    EventLoopThreadPool(EventLoop *baseLoop, const std::string &nameArg);
    ~EventLoopThreadPool();

    void setThreadNum(int numThreads) { numThreads_ = numThreads;}     //TcpServer的setThreadnumber也是调用的这个setThreadnumber

    void start(const ThreadInitCallback &cb = ThreadInitCallback());

    //如果工作在多线程中，baseLoop_默认以轮训的方式纷飞channel给subloop
    EventLoop* GetNextLoop();

    std::vector<EventLoop*> getAllLoops();

    bool started() const { return started_; }
    const std::string name() const { return name_;}
private:

    EventLoop *baseLoop_;
    std::string name_;
    bool started_;
    int numThreads_;
    int next_;     //做下一个loop的下表用的；
    std::vector<std::unique_ptr<EventLoopThread>> threads_;  //包含了Loop线程
    std::vector<EventLoop*> loops_;  //包含了所有loop的指针; (还记得调用EventLoopThread里边的startLoop()就能得到loop指针)
                           
};