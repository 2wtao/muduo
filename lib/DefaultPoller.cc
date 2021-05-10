//不能把设置DefaultPoller放在Poller基类中，因为它要获取PollPoller对象和EPollPoller对象
//派生类可以引用基类的，但是基类不能引用派生类的，不可能在一个抽象类里边include它的派生类；
#include "Poller.h"
#include "EPollPoller.h"

#include<stdlib.h>

Poller *Poller::newDefaultPoller(EventLoop *loop)
{
    if(getenv("MUDUO_USE_POLL"))
    {
        return nullptr; //生成poll的实例
    }
    else{
        return EPollPoller::newDefaultPoller(loop); //生成epoll的实例
    }
}
