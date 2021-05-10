#include"CurrentThread.h"

namespace CurrentThread
{
   __thread int t_cachedTid = 0;  //用这个变量临时存储线程id
   
   void cacheTid()
   {
       if (t_cachedTid == 0)
       {  
          //通过系统调用获取了当前线程的tid值
          t_cachedTid = static_cast<pid_t>(syscall(SYS_gettid)); 
       }
   }
}
