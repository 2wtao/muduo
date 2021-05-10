#include"TcpServer.h"
#include"Logger.h"
#include"Acceptor.h"
#include"TcpConnection.h"
#include"EventLoop.h"

#include <functional>
#include <strings.h>

static EventLoop* checkLoopNotNull(EventLoop *loop)
{
    if(loop == nullptr)
    {
        LOG_FATAL("%s:%s:%d mainLoop is null! \n", __FILE__, __FUNCTION__, __LINE__);
    }
    return loop;
}

TcpServer::TcpServer(EventLoop *loop,     
              const InetAddress &listenAddr,
              const std::string &nameArg,
              Option option)
              : loop_(checkLoopNotNull(loop))
              , ipPort_(listenAddr.toIpPort())
              , name_(nameArg)
              , acceptor_(new Acceptor(loop, listenAddr,option == kNoReusePort))
              , threadPool_( new EventLoopThreadPool(loop, name_))
              , connectionCallback_()
              , nextConnId_(1)
{

    //当Poller监听到有新连接事件发生时，执行handread回调操作（先accept返回一个connfd，
    //然后再执行newConnctionCallback，，通过轮训算法选择一个subLoop，
    //唤醒它并且把当前connfd封装成channel分发给subloop）


    //当有新用户链接时，会执行newconnection回调
    acceptor_->setNewConnectionCallback(std::bind(&TcpServer::newConnection,this,
                                        std::placeholders::_1, std::placeholders::_2));
}

TcpServer::~TcpServer()
{
    for (auto &item : connections_)    //遍历保存链接的map表
    {
        //释放掉Connection类型的指针后，利用这个临时指针去访问指针的对象，去销毁链接(connection),秒啊
        TcpConnectionPtr conn(item.second); 
        item.second.reset();

        conn->getloop()->runInloop(
           std::bind(&TcpConnection::connectDestoryed,conn)
        );
    }
}

void TcpServer::setThreadNum(int numThreads)  //设置底层EventLoop的数量
{
   threadPool_->setThreadNum(numThreads);
}

void TcpServer::start() //开启服务监听
{
   if(started_++ ==0)  //防止一个TcpServer对象被start多次
   {
       threadPool_->start(threadInitCallback_);    //启动底层线程池
       loop_->runInloop(std::bind(&Acceptor::listen,acceptor_.get()));
   }
}


//当有一个新的客户端的链接，acceptor会执行这个回调操作
void TcpServer::newConnection(int sockfd, const InetAddress &peerAddr)  //增加新连接
{
   //通过轮训算法，选择一个subLoop，来管理channel
   EventLoop *ioLoop = threadPool_->GetNextLoop();  //
   char buf[64] = {0};
   snprintf(buf, sizeof buf, "-%s#%d",ipPort_.c_str(), nextConnId_);
   ++nextConnId_;    //这个nextConnId不是原子型的是因为只在一个线程中执行
   std::string connName = name_ + buf;

   LOG_INFO("TcpConnection::newConnection [%s] - new connection [%s] from %s\n",
            name_.c_str(), connName.c_str(), peerAddr.toIpPort().c_str());

   //通过sockfd获取其绑定的本机的ip地址和端口信息
   sockaddr_in local;
   ::bzero(&local, sizeof(local));
   socklen_t addrlen = sizeof(local);
   if(::getsockname(sockfd, (sockaddr*)&local, &addrlen) < 0)
   {
       LOG_ERROR("sockets::getLocalAddr");
   }
   
   InetAddress localAddr(local);
   
   //根据链接成功的sockfd，创建TcpConnection连接对象
   TcpConnectionPtr conn(new TcpConnection(
                             ioLoop,
                             connName,
                             sockfd,   //通过链接成功的sockfd，底层创建socket、和对应的channel
                             localAddr,
                             peerAddr));

    connections_[connName] = conn; //这个unorered_map   保存名字和对应的连接  
    // 下面的回调都是用户设置给TcpServer ====> TcpConnection ====> Channel ===> Poller (notify)channel（调用回调）
    //                              (设置给)             (设置给)       (设置给)
    conn->setConnectionCallback(connectionCallback_);
    conn->setMessageCallback(messageCallback_);
    conn->setWriteCompleteCallback(writeCompleteCallback_);

    //设置了如何关闭链接的回调
    conn->setCloseCallback(
        std::bind(&TcpServer::removeConnection, this, std::placeholders::_1)
    );

    //直接调用这个方法，也就是说，有新用户链接后就直接调用这个方法
    ioLoop->runInloop(std::bind(&TcpConnection::connectEstablished, conn));  //执行创建新连接的回调
}


void TcpServer::removeConnection(const TcpConnectionPtr &conn)  //删除链接
{
    loop_->runInloop(
        std::bind(&TcpServer::removeConnectionInloop, this, conn)
    );
}

void TcpServer::removeConnectionInloop(const TcpConnectionPtr &conn) //在loop中删除该链接
{
    LOG_INFO("TcpServer::removeConnectionInLoop [%s] - connection \n",
     name_.c_str(),conn->name().c_str());
    
    size_t n = connections_.erase(conn->name());    //讲该connection从无序map表里边删除，拿名字做key  //返回值不关紧要，返回的是删除的个数
    EventLoop *ioLoop = conn->getloop();   //得到这个链接所在的loop
    ioLoop->queueInLoop(
      std::bind(&TcpConnection::connectDestoryed, conn)
    );

}
 











    // MessageCallback messageCallback_;      //有读写事件的回调
    // WriteCompleteCallback writeCompleteCallback_; //消息发送完成以后的回调

    // ThreadInitCallback threadInitCallback_; //loop线程初始化的回调

    // std::atomic_int started_;

    // int nextConnId_;
    // ConnectionMap connections_; //保存所有的链接