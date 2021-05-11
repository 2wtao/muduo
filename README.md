## muduo网络库的核心代码块

#### Channel

************

fd、events、revents、callbacks   两种channel  listenfd--acceptorChannel    connfd--connectionChannel

​                      listenfd有事件发生，执行的回调函数肯定Acceptor给Channel的           connfd是Tcpconnection给的           

*****************

封装了fd ------- poller监听的对象

events（事件）-------注册fd感兴趣的事件，根据事件返回相对应的回调操作

revnets（就绪事件）---------poller返回的具体发生的事件

还有一组回调函数（读、写、关闭、错误）

> 不管是新用户连接的fd，还是已连接用户读写事件的fd，都会被打包成channel，注册到**相应**Loop的poller上面。

**如果有fd，会将fd打包成channel，添加到poller里边的channel map表<fd(int),channel*>**

[channel和poller不能直接通信，需要通过eventloop来进行通信]()



#### Poller和EpollPoller---------Demultiplex（事件分发器）

************************

std::unordered_map<int,cahnnel*> channels

************************************

poller检查到fd上有时间发生了，可以通过fd找到对应的channel，channel里边就有具体的回调函数



#### EventLoop-------Reactor（反应堆）

**********************************

ChannelList activeChannels_ (vector)                 

std::unique_ptr<Poller> poller_;

 **weakfd**----->loop

和**std::unique<channel>weakupChannel**

*************************************

-eventloop收到poller返回的发生事件还得执行相关回调，因此EventLoop保存的是channel

-**当每个loop执行Poller时候（以EpollPller为例）在执行epoll时候都是阻塞状态**

-这时候用loop对象给wakefd上写个东西，相应的Loop就会被唤醒

每个weakupFd也被封装成channel在EventLoop的channel   vector中，

#### 以上三个密切相关

***********************

#### Thread 和 EventLoopThead

EventLoopThread::startLoop    开启一个线程，若线程对用的loop指针为空，则线程阻塞

等待EventLoopThread::threadFunc，创建一个独立的eventloop，和上面的线程一一对应（**这里是用loop_先等于threadFunc新创建的Loop，再用startLoop里边对应的Loop指针指向loop_(成员变量全局的)**）

***************

#### EventLoopThreadPool

getNextLoop()  :通过轮询算法获取下一个subloop，如果没有subLoop，永远返回baseLoop

one  Loop  per Thread

***************************************

#### Socket

#### Acceptor

主要封装了listenfd的相关操作， socket  bind  listen

把listenfd打包成acceptorChannel，并传给相应的回调操作，添加到baseLoop里边监听

****************

#### Buffer

缓冲区  应用写数据---->buffer缓冲区---->Tcp缓冲区---->send

​        prependable     readerindex    writeindex

​                 8字节         可发送下标        可写下标

#### TcpConnection

一个连接成功的客户端对应一个TcpConnection

socket    channel    各种回调     发送和接受缓冲区



#### TcpServer

总领所有

********************

Acceptor    EventLoopThreadPool    

ConnectionMap  connections_





