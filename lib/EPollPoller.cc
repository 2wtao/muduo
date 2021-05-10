#include "EPollPoller.h"
#include "Logger.h"
#include "Channel.h"

#include <errno.h>
#include <unistd.h>
#include <string.h>

const int kNew = -1;   //表示没在Poller添加过的channel
//channel成员的index初始化也是-1
const int kAdded = 1;   //表示已经添加过的channel
const int kDeleted = 2;   //表示已经删除的channel

EPollPoller::EPollPoller(EventLoop *loop)
    :Poller(loop)  //先初始化父类
    ,epollfd_(epoll_create1(EPOLL_CLOEXEC))
    ,events_(kInitEventListSize)
{
    if(epollfd_ < 0)
    {
        LOG_FATAL("epoll_create error;%d \n",errno);
    }
}
EPollPoller::~EPollPoller() 
{
    close(epollfd_);
}

Timestamp EPollPoller::poll(int timeoutMs,ChannelList *activeChannels)  //记住ChannleList只是一种类型，poll返回的发生的事件都在另一个ChannelList里边
{
    //实际上用LOG_DEBUG更为合理，效率问题
    LOG_DEBUG("func=%s => fd total count:%d\n",__FUNCTION__,channels_.size());

    int numEvents = epoll_wait(epollfd_,&*events_.begin(),static_cast<int>(events_.size()),timeoutMs);
    int saveErrno = errno;
    Timestamp now(Timestamp::now());

    if (numEvents > 0)
    {
       LOG_INFO("%d events happened\n",numEvents);
       fillActiveChannels(numEvents, activeChannels);
       if (numEvents == events_.size())
       {
           events_.resize(events_.size() * 2);
       }
    }
    else if (numEvents == 0) //表示这一轮循环没有要发生的事，只是timeout超时了
    {
        LOG_DEBUG("%s timeout! \n", __FUNCTION__);
    }
    else
    {
        if (saveErrno != EINTR)
        {
            errno = saveErrno;
            LOG_ERROR("EPollPoller::Poll() err!");
        }
    }
    return now;
}

//channel的update  remove => EventLoop的 updateChannel  removeChannel => Poller
/**
 * EventLoop里边有个ChannelList，向Poller注册过和没注册过的和没注册过的所有的channel都在ChannelList里边；
 *                     EventLoop =>
 *            ChannelList          Poller 
 *                              ChannelMap <fd, channel*>
 */
void EPollPoller::updateChannel(Channel *channel)
{
    const int index = channel->index();
    LOG_INFO("func=%s => fd=%d evnets=%d index=%d \n",__FUNCTION__,channel->fd(),channel->evnets(),index);

    if(index == kNew || index == kDeleted)
    {
        if(index == kNew)
        {
            int fd = channel->fd();
            channels_[fd] = channel;  //往poller里边的map表里添加fd和对应的channel;
        }

        channel->set_index(kAdded);    //状态改为已添加
        update(EPOLL_CTL_ADD, channel);
    }
    else
    {
        int fd = channel->fd();
        if(channel->isNoneEvent())
        {
            update(EPOLL_CTL_DEL, channel);
            channel->set_index(kDeleted);
        }
        else{
            update(EPOLL_CTL_MOD, channel);
        }
    }
}

//从Poller中删除channel
void EPollPoller::removeChannel(Channel *channel)
{
    int fd = channel->fd();
    channels_.erase(fd);

    //加日志的好处，能定位问题
    LOG_INFO("func=%s => fd=%d",__FUNCTION__,fd);

    int index = channel->index();
    if (index == kAdded)
    {
       update(EPOLL_CTL_DEL,channel);
    }
    channel->set_index(kNew);
}

//填写活跃的channel在activeChannel(ChannelList)里边
void EPollPoller::fillActiveChannels(int numEvents, ChannelList *activeChannel) const
{
    for (int i=0;i<numEvents;++i)
    {
        Channel *channel = static_cast<Channel*>(events_[i].data.ptr);   //date.fd就是fd，date.ptr就是channel
        channel->set_revents(events_[i].events);
        activeChannel->push_back(channel);  //EventLoop就拿到所有发生事件的列表了
    }
  
}

//更新channel通道  epoll_ctl add/mod/del
void EPollPoller::update(int operation, Channel *channel)
{
    epoll_event event;
    memset(&event, 0, sizeof(event));   //bzero(&event,sizef(event));
    
    int fd = channel->fd();
    
    event.events = channel->evnets();
    event.data.fd = fd;
    event.data.ptr = channel;
    

    if(epoll_ctl(epollfd_, operation, fd, &event) < 0)
    {
        if(operation == EPOLL_CTL_DEL)
        {
            LOG_ERROR("epoll_ctl_del error:%d\n",errno);
        }
        else
        {
            LOG_FATAL("epoll_ctl_add/mod error:%d\n",errno);   //FATAL之后会自动退出
        }
    }
}
