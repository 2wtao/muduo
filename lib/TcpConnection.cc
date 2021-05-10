#include "TcpConnection.h"
#include "Logger.h"       //一般大类、复杂的类错误容易发生、且较多，因此加上日志，方便错误的一个追踪
#include "Socket.h"
#include "Channel.h"
#include "EventLoop.h"

#include <functional>
#include <errno.h>
#include <memory>
#include <sys/types.h>        
#include <sys/socket.h>
#include <string.h>
#include <netinet/tcp.h>
#include <string>

static EventLoop* checkLoopNotNull(EventLoop *loop)
{
    if(loop == nullptr)
    {
        LOG_FATAL("%s:%s:%d TcpConnection Loop is null! \n", __FILE__, __FUNCTION__, __LINE__);
    }
    return loop;
}

TcpConnection::TcpConnection(EventLoop *loop
                 ,const std::string &nameArg
                 ,int sockfd
                 ,const InetAddress& localAddr
                 ,const InetAddress& peerAddr)
                 : loop_(checkLoopNotNull(loop))
                 , name_(nameArg)
                 , state_(kConnecting)
                 , reading_(true)
                 , socket_(new Socket(sockfd))
                 , channel_(new Channel(loop, sockfd))
                 , localAddr_(localAddr)
                 , peerAddr_(peerAddr)
                 , highWaterMark_(64*1024*1024)    //64M
                {
                    //这些回调都是Poller通知channel时执行的回调
                    //（就是：给Channel设置相应的回调函数，Poller给Channel通知感兴趣的事件发生了，channel会只想相应的操作函数）
                    channel_->setReadCallback(
                        std::bind(&TcpConnection::handleRead,this,std::placeholders::_1)
                    );//channel绑定的读事件处理

                    channel_->setWriteCallback(
                        std::bind(&TcpConnection::handleWrite,this)
                    );//channel绑定的写事件处理

                    channel_->setCloseCallback(
                        std::bind(&TcpConnection::handleClose,this)
                    );//channel绑定的close事件处理

                    channel_->setErrorCallback(
                        std::bind(&TcpConnection::handleError,this)
                    );//channel绑定的错误事件处理

                    LOG_INFO("TcpConnection::ctor[%s] at fd=%d\n", name_.c_str(),sockfd);
                    socket_->setKeepAlive(true);   //what‘s this
                }
TcpConnection::~TcpConnection() 
{
    LOG_INFO("TcpConnection::dtor[%s] at fd=%d state=%d \n",
             name_.c_str(),channel_->fd(),(int)state_);
}

void TcpConnection::send(const std::string &buf)
{
   if (state_ == kConnected)  //想要发送数据，必须是已建立链接的状态
   {
      if (loop_->isInLoopThread())
      {
         sendInLoop(buf.c_str(),buf.size());
      }
      else
      {
         loop_->runInloop(std::bind(
             &TcpConnection::sendInLoop,
             this,
             buf.c_str(),
             buf.size()
         ));
      }
   }
}

/**
 * 发送数据  应用写的快，而内核发送数据慢， 需要把待发送数据写入缓冲区。且设置了水位回调
 * 
 */ 
void TcpConnection::sendInLoop(const void* data, size_t len)
{
    ssize_t nwrote = 0;
    size_t remaining = len; //未发送完的数据；
    bool faultError = false; //是否错误

    //已经断开链接了，说明之前调用过该connection的shutdown，不能在进行发送了
    if(state_ == kDisconnected)
    {
       LOG_ERROR("disconnected,give up writing");
       return;
    }

    //表示channel_是第一次开始写数据，而且缓冲区没有待发送数据
    if(!channel_->isWriting() && outputBuffer_.readableBytes() == 0)
    {
       nwrote = ::write(channel_->fd(),data,len);  //返回的具体发送的个数
       {
          remaining = len - nwrote;
          if (remaining == 0 && writeCompleteCallback_)  //
          {
              //既然在这里数据全部发送完成，就不用再给channel设置epollout事件了
              loop_->queueInLoop(
                  std::bind(writeCompleteCallback_, shared_from_this())
              );
          }
       }
    }
    else   //nwrote < 0
    {
         nwrote = 0;
         if (errno != EWOULDBLOCK)  //由于非阻塞没有数据正常的返回
         {
            LOG_ERROR("TcpConnection::sendInLoop");
            if (errno == EPIPE || errno == ECONNRESET)
            {
                faultError = true;
            }
         }
    }

    //说明当前这一个write，并没有把数据全部发出去，剩余的数据需要保存到缓冲区中
    //注册epollout事件，poller发现tcp的发送缓冲区有空间，会通知相应的sock-channel，调用writeCallback_回调方法
    //也就是调用TcpConnect::handleWrite方法，把缓冲区中的数据全部发送完成
    if(faultError && remaining > 0)  
    {
      //目前发送缓冲区剩余的待发送数据的长度
      size_t oldLen = outputBuffer_.readableBytes();
      if (oldLen + remaining >= highWaterMark_
          && oldLen < highWaterMark_ 
          && highWaterMarkCallback_)
          {
             loop_->queueInLoop(
                 std::bind(highWaterMarkCallback_,shared_from_this(),oldLen+remaining)
             );
          }
          outputBuffer_.append((char*)data + nwrote, remaining); //剩余的数据保存到缓冲区中
          if(!channel_->isWriting())
          {
            channel_->enableReading(); //这里一定要注册写事件，否则epoller不给channel通知epollout
          }
    }
}


void TcpConnection::handleRead(Timestamp receiveTime)
{
    int savedErrno = 0;
    ssize_t n = inputBuffer_.readFd(channel_->fd(),&savedErrno);
    if(n>0)  //读到数据了
    {
        // 已建立的用户，有可读事件发生了，调用用户传入的回调操作；
        messageCallback_(shared_from_this(), &inputBuffer_, receiveTime);
    }
    else if (n==0)  //对端断开链接了
    {
        handleClose();
    }
    else  //出错
    {
        errno = savedErrno;
        LOG_ERROR("TcpConnection::handleRead");
        handleError();
    }
}

void TcpConnection::handleWrite()
{
    if(channel_->isWriting())
    {
        int savedErrno = 0;
        ssize_t n = outputBuffer_.writeFd(channel_->fd(), &savedErrno);
        if(n > 0)
        {
            outputBuffer_.retrieve(n);   //复位了，处理完了
            if(outputBuffer_.readableBytes() == 0)  //发送完成
            {
               channel_->disableReading();
               if(writeCompleteCallback_)
               {
                 loop_->queueInLoop(
                     std::bind(writeCompleteCallback_, shared_from_this())
                 );
               }
               if(state_ == kDisconnecting)
               {
                   shutdownInLoop();
               }
            }
        }
        else{
            LOG_ERROR("TcpConnection::handlewrite");
        }
    }
    else{
        LOG_ERROR("TcpConnection fd=%d is down,no more writing\n",channel_->fd());
    }
}


//poller ==> cahnnel::closeCallback ===> TcpConnection::handleClose
void TcpConnection::handleClose()
{
  LOG_INFO("fd=%d state=%d \n",channel_->fd(),(int)state_);
  setState(kDisconnected);
  channel_->disableAll();  //对所有channel都不感兴趣了

  TcpConnectionPtr connPtr(shared_from_this());
  connectionCallback_(connPtr);  //执行链接关闭的回调
  closeCallback_(connPtr);   //关闭链接的回调     //执行的是TcpServer::removeConnection回调方法
}

void TcpConnection::handleError()
{
   int optval;
   socklen_t optlen = sizeof optval;
   int err = 0;
   if(::getsockopt(channel_->fd(),SOL_SOCKET,SO_ERROR,&optval,&optlen) < 0)
   {
      err = errno;
   }
   else{
       err = optval;
   }
   LOG_ERROR("TcpConnection::handleError name:%s - SO_ERROR:%d \n",name_.c_str(),err);
}

// 链接建立  //channel的操作要依赖于TcpConnection，防止用户的非法操作关闭Connection，
//因此需要tie这样一个弱智能指针来对每次操作进行提升，提升失败说明Connection已经不存在；
void TcpConnection::connectEstablished()
{
   setState(kConnected); //已经成功链接
   channel_->tie(shared_from_this());
   channel_->enableReading();   //向Poller注册channel的epollin事件
   
   //新连接建立，执行回调
   connectionCallback_(shared_from_this());
}
// 链接销毁
void TcpConnection::connectDestoryed()
{
   if(state_ == kConnected)
   {
      setState(kDisconnected);
      channel_->disableAll();  //把channel的所有感兴趣的事件，从poller中del(其实就是把events改为 kNoneEvnet)
      connectionCallback_(shared_from_this());
   }
   channel_->remove();  //把channel从poller中删除
}

//关闭链接
void TcpConnection::shutdown()
{
   if(state_ == kConnected)
   {
       setState(kDisconnecting);
       loop_->runInloop(
           std::bind(&TcpConnection::shutdownInLoop, this)
       );
   }
}

void TcpConnection::shutdownInLoop()
{
   if(!channel_->isWriting())       //没有写事件，说明发送缓冲区的数据已经完了,当前outputbuff已全部发送完成
   {
     socket_->shutdownWrite();   //关闭写端
   }
}