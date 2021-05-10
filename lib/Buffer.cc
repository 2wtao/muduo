#include "Buffer.h"

#include <sys/uio.h>
/**
 * 从fd上读取数据  Poll工作在LT模式
 * Buffer缓冲区是有大小的！但是从fd还是那个读取数据的时候，却不知道tcp数据最终的大小
 */ 

ssize_t Buffer::readFd(int fd,int* saveErrno)
{
   char extrabuff[65536] = {0};   //栈上的空间   64K

   struct iovec vec[2];    //可以往不同的缓冲区填充数据，参数表示填充缓冲区的个数，方便回头改

   const size_t writable = writableBytes();  //Buffer底层缓冲区剩余的可写空间的大小
   
   vec[0].iov_base = begin() + writerIndex_;  //起始地址
   vec[0].iov_len = writable;                 //填充大小

   vec[1].iov_base = extrabuff;
   vec[1].iov_len = sizeof(extrabuff);

   const int iovcnt = (writable < sizeof(extrabuff))? 2:1;  //Buffer底层可写缓冲区大小  和  辅助缓冲区大小的比较
   const ssize_t n = ::readv(fd, vec, iovcnt);
   if(n < 0 )
   {
       *saveErrno = errno;
   }
   else if(n <= writable)
   {
       writerIndex_ += n;
   }
   else  //n > writable  extrabuff也写入了数据
   {
       writerIndex_ = buffer_.size();
       append(extrabuff, n-writable);
   }
   return n;
}

ssize_t Buffer::writeFd(int fd,int* saveErrno)
{
    ssize_t n = ::write(fd, peek(), readableBytes());
    if(n < 0)
    {
        *saveErrno = errno;
    }
    return n;
}