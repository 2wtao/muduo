#pragma once

#include"noncopyable.h"
#include"Timestamp.h"

#include <vector>
#include <unordered_map>

class Channel;
class EventLoop;

//muduo库中多路事件分发器的核心IO复用模块
class Poller:noncopyable
{
public:
    using ChannelList = std::vector<Channel*>;

    Poller(EventLoop *loop);
    virtual ~Poller();


    //给所有的IO复用保留统一的接口,需要它们去重写的;
    //当前激活的channel，运行的channel，需要Poller去照顾的channel都在ChannelList里边;
    virtual Timestamp poll(int timeoutMs,ChannelList *activeChannels) = 0;
    virtual void updateChannel(Channel *channel) = 0;
    virtual void removeChannel(Channel *channel) = 0;
    
    //判断一个Poller是否拥有一个channel
    bool hasChannel(Channel *channel) const;
    
    //EventLoop可以通过该接口获取默认的IO复用的具体实现
    static Poller *newDefaultPoller(EventLoop *loop);
protected:
    //map的key：sockfd     value：sockfd所属的channel通道类型
    using ChannelMap = std::unordered_map<int,Channel*>;  //poller监听的channel是EventLoop（EventLoop里边有ChannelList）里边保存的channel
    ChannelMap channels_;
private:
    EventLoop *ownerLoop_; //定义Poller所属的事件循环EventLoop
};