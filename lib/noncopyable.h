#pragma once    //防止头文件重复包含

/*
  *noncopyable被继承后，派生类可以正常的构造和析构，
  *但是派生类对象无法进行拷贝构造和赋值操作
*/
class noncopyable{
public:
      noncopyable(const noncopyable&) = delete;
      noncopyable& operator=(const noncopyable&) = delete;   //与ANBAN返回值是void，区别就是能不能重复赋值  a=b=c,现在既然已经都delete了，就不在乎了
protected:
      noncopyable() = default;
      ~noncopyable() = default;
};