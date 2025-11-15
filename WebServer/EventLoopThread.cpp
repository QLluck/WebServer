/// @file EventLoopThread.cpp
/// @brief 事件循环线程类实现文件
/// @author Lin Ya
/// @email xxbbb@vip.qq.com
/// 
/// @details 实现了EventLoopThread类的所有方法

#include "EventLoopThread.h"
#include <functional>  ///< 函数对象和bind

/**
 * @brief EventLoopThread构造函数
 * 
 * @details 初始化成员变量，创建线程对象（绑定threadFunc），但尚未启动
 */
EventLoopThread::EventLoopThread()
    : loop_(NULL),
      exiting_(false),
      thread_(bind(&EventLoopThread::threadFunc, this), "EventLoopThread"),
      mutex_(),
      cond_(mutex_) {}

/**
 * @brief EventLoopThread析构函数
 * 
 * @details 如果EventLoop已创建，退出事件循环并等待线程结束
 */
EventLoopThread::~EventLoopThread() {
  exiting_ = true;
  if (loop_ != NULL) {
    loop_->quit();
    thread_.join();
  }
}

/**
 * @brief 启动线程并返回EventLoop指针
 * 
 * @return EventLoop* 线程中的EventLoop指针
 * 
 * @details 执行流程：
 * 1. 启动线程
 * 2. 使用条件变量等待EventLoop创建完成
 * 3. 返回EventLoop指针
 * 
 * @note 必须在调用startLoop()后才能使用返回的EventLoop指针
 */
EventLoop* EventLoopThread::startLoop() {
  assert(!thread_.started());
  thread_.start();
  {
    MutexLockGuard lock(mutex_);
    // 一直等到threadFunc在Thread里真正跑起来并创建EventLoop
    while (loop_ == NULL) cond_.wait();
  }
  return loop_;
}

/**
 * @brief 线程函数，创建EventLoop并进入事件循环
 * 
 * @details 在新线程中执行：
 * 1. 创建EventLoop对象（栈上）
 * 2. 将EventLoop指针赋值给loop_，通知等待的线程
 * 3. 进入事件循环（阻塞调用）
 * 4. 事件循环退出后，清空loop_指针
 */
void EventLoopThread::threadFunc() {
  EventLoop loop;  // 栈上创建EventLoop

  {
    MutexLockGuard lock(mutex_);
    loop_ = &loop;
    cond_.notify();  // 通知等待的线程EventLoop已创建
  }

  loop.loop();  // 进入事件循环（阻塞调用）
  // assert(exiting_);
  loop_ = NULL;  // 事件循环退出后清空指针
}