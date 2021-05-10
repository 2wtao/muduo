#pragma once

#include<unistd.h>
#include<sys/syscall.h>

namespace CurrentThread
{
    extern __thread int t_cachedTid;

    void cacheTid();

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
}