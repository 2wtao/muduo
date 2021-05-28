邮箱：2877944240@qq.com

> 在源码的lib目录下有个autobulid.sh自动编译脚本文件，直接执行就可以了

## muduo的模型设计

#### Reactor模型

首先得明白reactor和proactor模型的区别

> **reactor**：用于同步IO，同步IO就是说，当内核缓冲区有数据时，IO函数被唤醒，IO函数一执行，开始从内     ​                 核缓冲区向用户缓冲区拷贝数据，所耗费的时间都记在程序用时；
>
> **proactor**：用于异步IO，异步IO就是说用户把sockfd、buf、和通知方式（不止信号还有回调）通过IO接口
> ​                    传给内核，当内核缓冲区有数据时，IO函数被唤醒，IO函数一执行内核缓冲区就有数据；内核会
> ​                    把数据拷贝到用户提供的buf中，并按照用户提供的通知方式应用程序，这个拷贝的过程，全程都
> ​                   是内核完成的，不计入应用程序耗时；

Reactor是由**Event(事件)、Reactor(反应堆)、Demultiplex(事件分发器)和Evanthandler(事件处理器)**四部分构成的

![1622117752496](C:\Users\86155\AppData\Roaming\Typora\typora-user-images\1622117752496.png)

**Event**：注册事件和其对应的处理方法给**Reactor**，**Reactor**里边有**sockfd**以及其对应的**event**

**Reactor**：向其**Demultiplex**添加 ( 此处以add为例，其他方法也一样 )，启动反应堆

**Demultiplex**：开启事件循环，将发生事件的**event**返回给**Reactor**

**EventHandler**：**Reactor**调用**event**对应的事件处理器**EventHandler**执行相应的回调函数

#### muduo库的Reactor模型

* channel封装了Event
* EventLoop就是一个Reactor
* Poller就是一个Demultipex
* channel里边有对应的回调函数

![1622122115122](C:\Users\86155\AppData\Roaming\Typora\typora-user-images\1622122115122.png)

* **EvnetLoop**在一个单独的进程里边执行(**one  (event)loop  per  thread**)
* 一个**EventLoop**里边有一个**Poller**和多个**Channel**，其中**Poller**和**channel**不能相互调用，必须通过**EventLoop**来进行相关数据的传输
* **channel**需要调用**EventLoop**的方法，对**channelMap**里边的**channel**进行增删改查
* **Poller**监听**sockfd**，并把返回的有事件发生的fd对应的**channel**添加到**activechannel**里边
* **EventLoop**通知**channel**执行对应的回调

#### one  (evnet)loop  per  thread

​       **在muduo库里边有两种线程：一种里边的事件循环专门处理新用户连接，一种里边的事件循环专门处理对应连接的所有读写事件**

mainLoop( 也就是baseLoop): 负责处理新用户的连接

![1622124907054](C:\Users\86155\AppData\Roaming\Typora\typora-user-images\1622124907054.png)

* **Acceptor**主要封装了**listenfd**( 对应的新用户连接事件 )的相关操作，将**listenfd**封装成了**acceptChannel**，并注册到其**Poller**中，**Poller**监听到事件发生后，**mainLoop**通知**acceptChannel**执行相关回调（此回调是**Acceptor**传给的）

* 执行**Acceptor::handleread**时，先进行一个**accpte**操作，再执行一个回调，**newConnectioncallback**是**TcpServer**传给的，轮询选择一个subloop

* 将**connfd**封装成**channel**分发给**subloop**，此**loop**负责此**connfd**上的所有操作；

  ioLoop：负责处理连接上的各种操作；包括建立连接、断开连接以及发送数据、接收数据

​                   muduo库底层实现了一个buffer，负责数据的存储，用于接收和发送

# C++11重写muduo核心类代码

## muduo库的核心类代码

### 辅助类

#### Noncopyable

此类主要做的就是通过下面两行代码，让其派生类不能进行拷贝构造和赋值，让代码更易读

```c++
      noncopyable(const noncopyable&) = delete;
      //让基类不能进行拷贝构造，从而使派生类也不能进行拷贝构造
      noncopyable& operator=(const noncopyable&) = delete;
      //让基类不能进行赋值运算，从而使派生类也不能进行赋值运算
```



#### Timestamp

此类主要为muduo库的一些log提供当前时间

```c++
     Timestamp();
     explicit Timestamp(int64_t microSecondsSinceEpoch);
     static Timestamp now();
     //获取当前的时间;直接return Timestamp(time(nullptr))
     std::string toString() const;
```



#### Logger

此类为一些操作提供相应日志信息，方便进行错误的追踪，采用的是饿汉模式(线程安全)

```c++
    INFO,  //普通信息
    ERROR, //错误信息
    FATAL, //core信息
    DEBUG, //调试信息
```

INFO：打印一些重要的流程信息

ERROR：不影响程序继续执行的错误信息

FATAL：影响程序继续执行的致命信息

DEBUG：调试信息

一共有四种级别的信息，每个函数都有其对应的宏函数LOG_INFO、LOG_ERROR、LOG_FATAL、LOG_DEBUG

```c++
#define [对应的宏](logmsgFormat, ...) \
    do \
    { \
        Logger &logger = Logger::instance();  \
        logger.setLogLevel([信息类型]);   \
        char buf[1024] = {0}; \
        snprintf(buf,1024,logmsgFormat,##__VA_ARGS__); \
        logger.log(buf); \
    } while(0) 
```



#### socket

该类主要封装了sockfd对应的操作

```c++
    int fd() { return sockfd_; }
    void bindAddress(const InetAddress &localaddr);
    void listen();
    int accept(InetAddress *peeraddr);
```



#### InetAddress

该类主要封装了socket的地址信息，可以得到ip地址和端口号

```c++
    explicit InetAddress(uint16_t port = 0,std::string ip = "127.0.0.1");
    explicit InetAddress(const sockaddr_in &addr)
        :addr_(addr)
        {}
    std::string toIp() const;   //得到ip
    std::string toIpPort() const;    //得到ip加端口号
    uint16_t toPort() const;   //得到端口号
    
    const sockaddr_in* getSockAddr() const{return &addr_;}
    void setSockaddr(const sockaddr_in &addr) { addr_ = addr;}
```


#### Buffer

muduo库底层的发送、接收的消息都是存放在Buffer缓冲区的

```c++
    static const size_t kCheapPrepend = 8;   //用来初始化首部信息
    static const size_t kInitialSize = 1024;   //用来初始化数据区
```

```C++
//构造函数
explicit Buffer(size_t initialSize = kInitialSize)     //initialSize是数据区大小·
             :buffer_(kCheapPrepend + kInitialSize)    //缓冲区的大小等于首部加数据区的大小
             ,readerIndex_(kCheapPrepend)              //可读区域的首地址下标
             ,writerIndex_(kCheapPrepend)              //可写区域的首地址下标
             {}
```

![1622183670567](C:\Users\86155\AppData\Roaming\Typora\typora-user-images\1622183670567.png)

 各种关系大体都是围绕这几个变量计算的

```C++
    //可读的大小
    size_t readableBytes() const 
    {
        return writerIndex_ - readerIndex_;
    }
```

```C++
    //可写的大小
    size_t writableBytes() const
    {
        return buffer_.size() - writerIndex_; 
    }
```



#### CurrentThread

该类通过系统调用获取当前线程的tid

```c++
    extern __thread int t_cachedTid;

    void cacheTid()
    {
       if (t_cachedTid == 0)
       {  
          //通过系统调用获取了当前线程的tid值
          t_cachedTid = static_cast<pid_t>(syscall(SYS_gettid)); 
       }
    }

    inline int tid()
    {
        //如果线程tid等于0就调用这个（通过系统调用获取线程tid的函数）来获取线程的tid
        //如果线程tid不等于0就返回它自己的；
        if (__builtin_expect(t_cachedTid == 0,0))
        {
           cacheTid();
        }
        return t_cachedTid;
    }
```



### TcpServer以及其相关类

#### TcpServer

```C++
    void newConnection(int sockfd, const InetAddress &peerAddr);  //增加新连接
    void removeConnection(const TcpConnectionPtr &conn);  //删除链接
    void removeConnectionInloop(const TcpConnectionPtr &conn); //在一个loop中删除该链接
 
    using ConnectionMap = std::unordered_map<std::string, TcpConnectionPtr>; //连接的名字和对应的连接


    EventLoop *loop_;     //默认的loop，即baseloop
    const std::string ipPort_;   //ip和端口号
    const std::string name_;    //名字

    std::unique_ptr<Acceptor> acceptor_;  //专门负责处理新用户的链接
    std::unique_ptr<EventLoopThreadPool> threadPool_;   //one loop per thread;

    ConnectionCallback connectionCallback_;  //有新链接的回调
    MessageCallback messageCallback_;      //有读写事件的回调
    WriteCompleteCallback writeCompleteCallback_; //消息发送完成以后的回调

    ThreadInitCallback threadInitCallback_; //loop线程初始化的回调

    std::atomic_int started_;

    int nextConnId_;
    ConnectionMap connections_; //保存所有的链接
```



```C++
    TcpServer(EventLoop *loop,     //默认的loop，即Baseloop  
              const InetAddress &listenAddr,    //地址信息
              const std::string &nameArg,    //名字
              Option option = kNoReusePort);   //端口是否重用
```

构造函数，主要传的参数就是前三个

```C++
    void setThreadInitcallback(const ThreadInitCallback &cb){ threadInitCallback_ = cb; }
//线程初始化回调函数
    void setConnectionCallback(const ConnectionCallback &cb){ connectionCallback_ = cb; }  //链接事件回调函数
    void setMessageCallback(const MessageCallback &cb) { messageCallback_ = cb; }
//读写事件回调函数
    void setWriteCompleteCallback(const WriteCompleteCallback &cb)          {writeCompleteCallback_ = cb; }
```

其中新用户链接的回调会传给Accptor

​       读写事件的回调会传给TcpConnection

**TcpServer主要的做的事就是**：

* setThreadNum

```c++
void TcpServer::setThreadNum(int numThreads) 
  //设置底层EventLoop的数量，就是ioLoop的数量，若数量为零，那connfd的读写事件也由baseLoop执行
{
   threadPool_->setThreadNum(numThreads);
}
```

* start

```C++
void TcpServer::start() //开启服务监听
{
   if(started_++ ==0)  //防止一个TcpServer对象被start多次
   {
       threadPool_->start(threadInitCallback_);    //启动底层线程池
       loop_->runInloop(std::bind(&Acceptor::listen,acceptor_.get()));
       //启动acceptor所在的loop，并且acceptor开始监听新用户的链接
   }
}
```

还有就是它负责实现Acceptor和TcpConnection所要调用回调

#### Acceptor

**此类主要处理新用户的链接，并且链接得到connfd，这个connfd里边的读写事件由TcpConnection执行**

 ![img](file:///C:\Users\86155\Documents\Tencent Files\2877944240\Image\C2C\33{GQ}R02~3QTGW$@Z_S`KO.png) 

还是这个图，Acceptor是在baseLoop里边执行的

```C++
void handleRead();
EventLoop *loop_;   //Accptor用的就是用户定义的那个baseLoop，也称mainLoop
Socket acceptSocket_;  //它负责监听的就是acceptSocket_上的事件
Channel acceptChannel_;   //将acceptSocket_封装成acceptChannel_
NewConnectionCallback newConnectionCallback_;   //当loop_里的poller发现acceptSocket_上有事件发生时，向loop_返回acceptChannel_，Acceptor执行此回调
bool listenning_;
```

acceptor的核心就是handleRead

```C++
void Acceptor::handleRead()
{
   InetAddress peerAddr;
   int connfd = acceptSocket_.accept(&peerAddr);
   if(connfd >= 0)
   {
       if(newConnectionCallback_)
       {
           newConnectionCallback_(connfd,peerAddr);
       }
       else
       {
           ::close(connfd);
       }
   }
   else{
       LOG_ERROR("%s:%s:%d accept err:%d \n",__FILE__,__FUNCTION__,__LINE__, errno);
       if(errno == EMFILE)
       {
          LOG_ERROR("%s:%s:%d reached limit! \n",__FILE__,__FUNCTION__,__LINE__, errno);
       }
   }

}
```

当有新用户链接时执行这个回调，**先accpet得到connfd，再执行newConnectioncallback（通过轮询的方式将connfd分发给ioLoop）**，当然这个回调都是传的TcpServer的方法，是在TcpServer里边实现的；

#### TcpConnection

**此类主要是负责connfd的各种操作，因此回调比较多，都是TcpServer传给的**；

```C++
    ConnectionCallback connectionCallback_;  //有新链接的回调
    MessageCallback messageCallback_;      //有读写事件的回调
    WriteCompleteCallback writeCompleteCallback_; //消息发送完成以后的回调
    HighWaterMarkCallback highWaterMarkCallback_; //高水位线
    CloseCallback closeCallback_;
```

```C++
    EventLoop *loop_;  //这里的loop绝对不是baseloop，因为TcpConnection都是在subloop里边管理的
    const std::string name_;  //Tcpconnection的名字
    std::atomic_int state_;   //链接的状态
    
    enum StateE {kDisconnected //已经断开的
                ,kConnecting  //正在链接
                ,kConnected  //已经连接上了
                ,kDisconnecting};  //正在断开 
    bool reading_;
```

由于Buffer是在发送、接收数据时用到的，因此Buffer是在TcpConnection里面用到的

### EventLoop以及其相关类

#### Channel

**此类负责打包sockfd（acceptfd和connfd）,不同fd对应的loop提醒channel执行的回调也是不同的**

acceptfd的回调是传的Acceptor的，对应的connfd的回调是传的TcpConnection的

它的主要成员变量如下

```C++
   EventLoop *loop_;  //事件循环（所属的事件循环）
   const int fd_;   //fd，就是poller监听的对象
   int events_;  //注册fd感兴趣的事件
   int revents_;  //发生的事件
   int index_;  //表示的就是channel的状态（未添加、已添加和已删除）
   //新创建的channel默认值为-1，通过updatechannel和removechannel的一个操作改变其对应的值
```

```C++
   void update();
   void remove();
   void handleEventWithGuard(Timestamp receiveTime);  //handle(处理) Event(事件) WithGuard(受保护的)
```

还有对Poller里边map表里边的channel_的管理是通过EventLoop对Poller执行的

因此这几个方法底层直接调用的EventLoop的对应方法

**channel就主要是封装了fd和其对应的回调**

```C++
   //因为Channel通道里边能够获知fd最终发生的具体的事件revent，所以它负责调用具体事件的回调操作；
   ReadEventCallback readCallback_;
   EventCallback writeCallback_;
   EventCallback closeCallback_;
   EventCallback errorCallback_;
```

#### Poller

此类是一个抽象接口类，向下提供不同的IO复用函数的接口

```C++
    //给所有的IO复用保留统一的接口,需要它们去重写的;
    //当前激活的channel，运行的channel，需要Poller去照顾的channel都在ChannelList里边;
    virtual Timestamp poll(int timeoutMs,ChannelList *activeChannels) = 0;
    virtual void updateChannel(Channel *channel) = 0;
    virtual void removeChannel(Channel *channel) = 0;

    using ChannelMap = std::unordered_map<int,Channel*>;  
    //poller监听的channel是EventLoop（EventLoop里边有ChannelList）里边保存的channel
```

#### EpollPoller

**此类是用EPollio复用实现的Poller，主要管理和监听channel，并将sockfd有事件发生的channel返回给EventLoop，由EventLoop通知channel执行相应的回调**

```c++
    //还负责重写基类的方法
    static const int kInitEventListSize = 16;  //保存事件的vector初始化大小
    
    //填写活跃的channel在activeChannel(ChannelList)里边
    void fillActiveChannels(int numEvents, ChannelList *activeChannel) const;
    //更新channel通道
    void update(int operation, Channel *channel);

    using EventList = std::vector<epoll_event>;  //事件集合

    int epollfd_;
    EventList events_; 
```

#### EventLoop

**此类相当于一个Reactor的角色，向Poller的map表里边注册fd事件对应的channel，并接受有事件发生的channel，并通知channel执行相应的回调**

```C++
    void handleRead();  //wakeup  通过给wakeupfd上写入数据，唤醒ioLoop
    void doPendingFunctors();  //执行回调

    using ChannelList = std::vector<Channel*>;   
    std::atomic_bool looping_;  //原子操作，底层通过CAS实现的
    std::atomic_bool quit_;  //标志退出loop循环

    const pid_t threadId_;  //记录当前loop所在线程id
    Timestamp pollReturnTime_;  //poller返回发生事件的channels的时间点
    std::unique_ptr<Poller> poller_;

    int wakeupFd_; //主要作用，当mainloop获取一个新用户的channel，通过轮训算法选择一个subloop，通过该成员唤醒subloop处理
    std::unique_ptr<Channel> wakeupChannel_;

    ChannelList activeChannels_;
    Channel *currentActiveChannel_;

    std::atomic_bool callingPendingFunctors_;  //标识当前loop是否需要执行的回调操作
    std::vector<Functor> pendingFunctors_;  //存储loop需要执行的所有的回调操作
    std::mutex mutex_; //互斥锁，用来保护上面vector容器的线程安全操作

```

至此muduo库重要的组件以及每个EventLoop实现的功能已经很清楚了，现在需要考虑的就是在多线程下，实现one  loop  per  thread

#### EventLoopThread

此方法是让一个loop在单独创建的线程中执行

```C++
    EventLoop *loop_;   //对应的loop
    bool exiting_;      //线程的状态
    Thread thread_;     //对应的线程
    std::mutex mutex_;   
    std::condition_variable cond_; 
    ThreadInitCallback callback_;
```

```C++
   
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
```

通过条件变量可以保证创建的线程和运行的loop一一对应

#### EventLoopThreadPool

还记得刚开始TcpServer设置了ThreadNum，在这个类中会根据设置的数量开启相应的线程，每个线程都和一个ioLoop一一对应，并且阻塞在recv等待被唤醒；

```C++
    EventLoop *baseLoop_;
    std::string name_;
    bool started_;
    int numThreads_;
    int next_;     //做下一个loop的下表用的；
    std::vector<std::unique_ptr<EventLoopThread>> threads_;  //包含了Loop线程
    std::vector<EventLoop*> loops_;  //包含了所有loop的指针; (还记得调用EventLoopThread里边的startLoop()就能得到loop指针)
```

```C++
    //TcpServer的两个方法底层就是调用者俩个方法就是调用的
    void setThreadNum(int numThreads) { numThreads_ = numThreads;}     
    void start(const ThreadInitCallback &cb = ThreadInitCallback())
    {
    started_ = true;
    for(int i=0; i<numThreads_; i++)
    {
       char buf[name_.size() + 32];
       snprintf(buf, sizeof buf, "%s%d", name_.c_str(),i);
       EventLoopThread *t = new EventLoopThread(cb,buf);
       threads_.push_back(std::unique_ptr<EventLoopThread>(t));  //根据开启的线程数开启相应的线程，
       loops_.push_back(t->startLoop());  //执行startLoop函数，返回loop指针，（没执行threadFunc灰调函数的话，线程会阻塞）
                           //底层创建线程，绑定一个新的EventLoop,并返回该loop的地址
    }
    if(numThreads_ == 0 && cb)
    {
        cb(baseLoop_);       //cb(ThreadInitCallback)（用户提前设置的回调）
    }
    }
```

在Acceptor的HandRead回调函数中，得到connfd，然后通过轮询的方法得到下一个loop，底层也是调用的Pool里边的GetNextLoop()方法；

```C++
//如果工作在多线程中，baseLoop_默认以轮训的方式纷飞channel给subloop
EventLoop* EventLoopThreadPool::GetNextLoop()
{
    EventLoop *loop = baseLoop_;
    if(!loops_.empty())
    {
       loop = loops_[next_];
       next_++;
       if(next_ >= loops_.size())
       {
        next_ = 0;
       }
    }
    return loop;
}
```

# 总结

核心代码到这里就基本拉下序幕了，我们可以再总结一下

* muduo库采用one loop per thread + thread pool的模型；

* 总体是基于Reactor模型的（EventLoop就是一个Reactor、Poller就是一个事件分发器）;

  sockfd以及其对应的回调操作被打包成channel，由EventLoop将其添加到Poller的map表里边，Poller将发生事件的channel返回在EventLoop的List表里边，EventLoop通知channel执行相应的回调操作；

* muduo库的TcpServer负责传入用户写的回调操作；

  此时fd分为两种p（acceptfd和connfd），分别和其对应的事件打包成channel，acceptfd感兴趣的事件是新用户的链接事件，因此acceptChannel被添加到baseLoop的Poller里边；

  当有新用户链接时，执行acceptfd对应的回调操作，并得到connfd，此时将connfd和其感兴趣的事件添加到ioLoop的Poller里边，当对应的事件发生时，调用相关的回调操作！