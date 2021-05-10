#pragma once

#include<string>
#include<atomic>

#include"noncopyable.h"

// LOG_INFO("%s %d",arg1,arg2)
#define LOG_INFO(logmsgFormat, ...) \
    do \
    { \
        Logger &logger = Logger::instance();  \
        logger.setLogLevel(INFO);   \
        char buf[1024] = {0}; \
        snprintf(buf,1024,logmsgFormat,##__VA_ARGS__); \
        logger.log(buf); \
    } while(0)  

#define LOG_ERROR(logmsgFormat, ...) \
    do \
    { \
        Logger &logger = Logger::instance();  \
        logger.setLogLevel(ERROR);   \
        char buf[1024] = {0}; \
        snprintf(buf,1024,logmsgFormat,##__VA_ARGS__); \
        logger.log(buf); \
    } while(0)  

#define LOG_FATAL(logmsgFormat, ...) \
    do \
    { \
        Logger &logger = Logger::instance();  \
        logger.setLogLevel(FATAL);   \
        char buf[1024] = {0}; \
        snprintf(buf,1024,logmsgFormat,##__VA_ARGS__); \
        logger.log(buf); \
        exit(-1);  \
    } while(0)  

#ifdef MUDEBUG
#define LOG_DEBUG(logmsgFormat, ...) \
    do \
    { \
        Logger &logger = Logger::instance();  \
        logger.setLogLevel(DEBUG);   \
        char buf[1024] = {0}; \
        snprintf(buf,1024,logmsgFormat,##__VA_ARGS__); \
        logger.log(buf); \
    } while(0)  
#else
    #define LOG_DEBUG(logmsgFormat, ...);
#endif

//定义日志的级别  INFO(打印一些重要的流程信息)
//              ERROR(不影响程序继续执行的错误)
//              FATAL(影响承租继续执行的错误)
//              DEBUG(调试信息:可选可不选；看需求)
enum LogLevel
{
    INFO,  //普通信息
    ERROR, //错误信息
    FATAL, //core信息
    DEBUG, //调试信息
};

//输出一个日志类
class Logger
{
public:
    //获取日志唯一的实例对象（懒汉模式）
    static Logger& instance();
    //设置日志级别
    void setLogLevel(int level);
    //写日志
    void log(std::string msg);
private:
    int logLevel_;  //类成员变量后面加_，防止和系统变量冲突；
    Logger() {}
};