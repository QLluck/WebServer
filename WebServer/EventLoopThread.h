/// @file EventLoopThread.h
/// @brief 事件循环线程类头文件
/// @author Lin Ya
/// @email xxbbb@vip.qq.com
/// 
/// @details EventLoopThread封装了一个线程和一个EventLoop，
/// 实现One Loop Per Thread模式

#pragma once
#include "EventLoop.h"        ///< 事件循环类
#include "base/Condition.h"  ///< 条件变量
#include "base/MutexLock.h"  ///< 互斥锁
#include "base/Thread.h"     ///< 线程类
#include "base/noncopyable.h" ///< 禁止拷贝基类

/**
 * @class EventLoopThread
 * @brief 事件循环线程类，封装一个线程和其EventLoop
 * 
 * @details 实现One Loop Per Thread模式：
 * - 每个线程拥有一个EventLoop
 * - 线程启动后创建EventLoop并进入事件循环
 * - 使用条件变量同步EventLoop的创建
 */
class EventLoopThread : noncopyable {
 public:
  /**
   * @brief 构造函数，初始化事件循环线程
   * 
   * @details 创建线程对象，但尚未启动
   */
  EventLoopThread();
  
  /**
   * @brief 析构函数，退出事件循环并等待线程结束
   */
  ~EventLoopThread();
  
  /**
   * @brief 启动线程并返回EventLoop指针
   * 
   * @return EventLoop* 线程中的EventLoop指针
   * 
   * @details 启动线程，等待EventLoop创建完成，返回EventLoop指针
   * 使用条件变量确保EventLoop已创建
   */
  EventLoop* startLoop();

 private:
  /**
   * @brief 线程函数，创建EventLoop并进入事件循环
   * 
   * @details 在新线程中创建EventLoop，通知等待的线程，然后进入事件循环
   */
  void threadFunc();
  
  EventLoop* loop_;     ///< 事件循环指针（线程局部）
  bool exiting_;        ///< 是否正在退出
  Thread thread_;       ///< 线程对象
  MutexLock mutex_;     ///< 保护loop_的互斥锁
  Condition cond_;      ///< 条件变量，用于同步EventLoop的创建
};