#include"Poller.h"
#include"Channel.h"

//记录Poller所属的EventLoop
Poller::Poller(EventLoop *loop)
    :ownerLoop_(loop) 
{
}

Poller::~Poller() = default;

//Poller在一个map里边存channel
bool Poller::hasChannel(Channel *channel) const
{
   auto it = channels_.find(channel->fd());
   return it != channels_.end() && it->second == channel;
}