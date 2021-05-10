#pragma once

#include <vector>
#include <unistd.h>
#include <string>

//网络库底层的缓冲器类型定义
class Buffer
{
public:
    static const size_t kCheapPrepend = 8;
    static const size_t kInitialSize = 1024;

    explicit Buffer(size_t initialSize = kInitialSize)
             :buffer_(kCheapPrepend + kInitialSize)
             ,readerIndex_(kCheapPrepend)
             ,writerIndex_(kCheapPrepend)
             {}
    /**
     * kCheapPrepend   |   readableBytes   |   writableBytes
    1.kCheapPrepend(初始校验码8) + 3.readerIndex_(可读下标) + 5.writeIndex_(可写下标)
    2.prependableBytes()(空闲的) + 4.readableBytes()(可读大小) + 6.writableBytes()(可写大小)
    */  


    //可读的大小
    size_t readableBytes() const    //可读的大小
    {
        return writerIndex_ - readerIndex_;
    }
    
    //可写的大小
    size_t writableBytes() const   //可写的大小
    {
        return buffer_.size() - writerIndex_; 
    }

    //空闲出来的大小
    size_t prependableBytes() const
    {
        return readerIndex_;
    }

    //返回缓冲区中可读数据的起始地址
    const char* peek() const
    {
        return begin() + readerIndex_;
    }

    //onMessage  string <= Buffer
    void retrieve(size_t len)
    {
        if(len < readableBytes())
        {
           readerIndex_ += len; //应用只读取了可读缓冲区的一部分（读了len长度），剩下就是readerIndex_ + len ~ writerIndex_;
        }
        else
        {
           retrieveAll();
        }
    }

    void retrieveAll()
    {
        readerIndex_ = writerIndex_ = kCheapPrepend; //该读的都读完了，该复位了，把可读可写都设置成标志位的大小
    }


    //把onMessage函数上报的Buffer数据，转成string类型的数据返回
    std::string retrieveAllAsString()
    {
       return retreveAsString(readableBytes()); //应用可读取数据的长度
    }

    std::string retreveAsString(size_t len)
    {
       std::string result(peek(), len);//可读区的起始地址加大小==可读的大小
       retrieve(len);
       return result;
    }

    //缓冲区可写的长度（buffer_.size() - writerIndex_）
    void ensureWritebaleBytes(size_t len)
    {
       if(writableBytes() < len)
       {
         makeSpace(len);  //扩容函数
       }
    }

    //把[data，data+len]的数据，添加到可写缓冲区中
    void append(const char* data, size_t len)
    {
       ensureWritebaleBytes(len);
       std::copy(data,data+len,beginWrite());
       writerIndex_ += len;  //可写的下标随着新数据的写入而向后推移
    }

    //开始写的地方
    char* beginWrite()
    {
        return begin() + writerIndex_;
    }
    
    //添加常方法，减少编译时的错误；
    const char* beginWrite() const
    {
        return begin() + writerIndex_;
    }

    //从fd上读数据
    ssize_t readFd(int fd,int* saveErrno);
    //通过fd发送数据
    ssize_t writeFd(int fd,int* saveErrno);
private:
    char* begin()
    {
        return &*buffer_.begin();
    }

    const char* begin() const
    {
        return &*buffer_.begin();
    }

    void makeSpace(size_t len)
    {
        if (writableBytes() + prependableBytes() < len + kCheapPrepend)
        {
            buffer_.resize(writerIndex_ + len);
        }
        else
        {
            size_t readable = readableBytes();  //可读的(未读的)
            std::copy(begin()+readerIndex_,begin()+writerIndex_,//(可读的大小)
                      begin()+kCheapPrepend); //把可读的部分移到 从8开始往后复制
            readerIndex_ = kCheapPrepend;  //这下可读就从8 开始；
            writerIndex_ = readerIndex_ + readable; //可写从可读加偏移量算起;
        }
    }

    std::vector<char> buffer_;
    size_t readerIndex_;
    size_t writerIndex_;
};