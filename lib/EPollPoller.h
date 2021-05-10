#pragma once

#include "Poller.h"
#include "Timestamp.h"

#include <vector>
#include <sys/epoll.h>

class Channel;
/**
 * epoll的使用
 * epoll_create
 * epoll_ctl  add/mod/del
 * epoll_wait
 */
class EPollPoller:public Poller
{
public:
    EPollPoller(EventLoop *loop);
    ~EPollPoller() override;
    
    //重写基类Poller的方法
    //相当于epoll_wait,返回的活跃channel在activeChannels(ChannelList)
    Timestamp poll(int timeoutMs,ChannelList *activeChannels) override;
    void updateChannel(Channel *channel) override;
    void removeChannel(Channel *channel) override;
private:
    static const int kInitEventListSize = 16;
    
    //填写活跃的channel在activeChannel(ChannelList)里边
    void fillActiveChannels(int numEvents, ChannelList *activeChannel) const;
    //更新channel通道
    void update(int operation, Channel *channel);

    using EventList = std::vector<epoll_event>;  //事件集合

    int epollfd_;
    EventList events_;
};