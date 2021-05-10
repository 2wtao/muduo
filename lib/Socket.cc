#include "Socket.h"
#include "Logger.h"
#include "InetAddress.h"

#include <unistd.h>
#include <sys/types.h>        
#include <sys/socket.h>
#include <string.h>
#include <netinet/tcp.h>

Socket::~Socket() 
{
   close(sockfd_);
}

void Socket::bindAddress(const InetAddress &localaddr)
{
    if (0 != bind(sockfd_,(sockaddr*)localaddr.getSockAddr(),sizeof(sockaddr_in)))
    {
        //成功则返回0，反之则失败
        LOG_FATAL("bind sockfd:%d fail \n",sockfd_);
    }
    
}

void Socket::listen()
{
    if(0 != ::listen(sockfd_, 1024))  //加作用域，防止和用户名字冲突
    {
        LOG_FATAL("listen sockfd:%d fail \n",sockfd_);
    }
}

int Socket::accept(InetAddress *peeraddr)
{
    sockaddr_in addr;
    socklen_t len;
    bzero(&addr,sizeof(addr));
    int connfd = ::accept(sockfd_, (sockaddr*)&addr, &len);  //给系统的方法加作用域
    if (connfd >= 0)
    {
        peeraddr->setSockaddr(addr);
    }
    return connfd;
}

void Socket::shutdownWrite()
{
    if(::shutdown(sockfd_, SHUT_WR) < 0)
    {
        LOG_ERROR("shutdownWrite error");
    }
}


void Socket::setTcpNoDelay(bool on)  //数据不进行tcp缓冲，直接发送//协议级别的
{
    int optval = on ? 1: 0;
    ::setsockopt(sockfd_, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(optval));
}
void Socket::setReuseAddr(bool on)  //socket级别的
{
    int optval = on ? 1: 0;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof optval);
}
void Socket::setReusePort(bool on)
{
    int optval = on ? 1: 0;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof optval);
}
void Socket::setKeepAlive(bool on)
{
    int optval = on ? 1: 0;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof optval);
}