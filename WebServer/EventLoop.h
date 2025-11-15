/// @file EventLoop.h
/// @brief 事件循环类头文件，Reactor模式的核心组件
/// @author Lin Ya
/// @email xxbbb@vip.qq.com
/// 
/// @details EventLoop是事件驱动模型的核心，每个线程拥有一个EventLoop实例
/// 负责IO事件的分发、定时器处理、跨线程任务执行等

#pragma once
#include <functional>          ///< 函数对象和回调
#include <memory>              ///< 智能指针
#include <vector>              ///< 动态数组容器
#include "Channel.h"          ///< 事件通道类
#include "Epoll.h"             ///< epoll封装类
#include "Util.h"              ///< 工具函数
#include "base/CurrentThread.h" ///< 获取当前线程ID
#include "base/Logging.h"      ///< 日志系统
#include "base/Thread.h"       ///< 线程相关

#include <iostream>
using namespace std;

/**
 * @class EventLoop
 * @brief 事件循环类，Reactor模式的核心组件
 * 
 * @details 每个线程拥有一个EventLoop实例，负责：
 * - IO事件的分发和处理
 * - 定时器的管理
 * - 跨线程任务的执行（通过eventfd唤醒）
 * - 待执行回调函数队列的处理
 * 
 * @note One Loop Per Thread设计模式，确保线程安全
 */
class EventLoop {
 public:
  typedef std::function<void()> Functor;  ///< 回调函数类型定义
  
  /**
   * @brief 构造函数，初始化事件循环
   * 
   * @details 创建epoll实例、eventfd用于跨线程唤醒、wakeup通道
   */
  EventLoop();
  
  /**
   * @brief 析构函数，清理资源
   */
  ~EventLoop();
  
  /**
   * @brief 开始事件循环（阻塞调用）
   * 
   * @details 主循环流程：
   * 1. 调用epoll_wait等待IO事件
   * 2. 处理活跃的IO事件
   * 3. 执行待处理的回调函数
   * 4. 处理超时的定时器
   * 循环直到quit()被调用
   */
  void loop();
  
  /**
   * @brief 退出事件循环
   * 
   * @details 设置quit_标志，如果不在事件循环线程中则唤醒
   */
  void quit();
  
  /**
   * @brief 在当前线程中执行回调函数
   * 
   * @param cb 要执行的回调函数
   * 
   * @details 如果当前线程就是事件循环线程，直接执行；
   * 否则加入队列，通过eventfd唤醒事件循环线程执行
   */
  void runInLoop(Functor&& cb);
  
  /**
   * @brief 将回调函数加入待执行队列
   * 
   * @param cb 要执行的回调函数
   * 
   * @details 线程安全地将回调函数加入队列，必要时唤醒事件循环
   */
  void queueInLoop(Functor&& cb);
  
  /**
   * @brief 判断当前线程是否是事件循环所属的线程
   * 
   * @return bool 如果是返回true，否则返回false
   */
  bool isInLoopThread() const { return threadId_ == CurrentThread::tid(); }
  
  /**
   * @brief 断言当前线程是事件循环所属的线程
   * 
   * @note 如果不是，程序会abort
   */
  void assertInLoopThread() { assert(isInLoopThread()); }
  
  /**
   * @brief 关闭通道的写端（优雅关闭）
   * 
   * @param channel 要关闭的通道
   */
  void shutdown(shared_ptr<Channel> channel) { shutDownWR(channel->getFd()); }
  
  /**
   * @brief 从epoll中移除通道
   * 
   * @param channel 要移除的通道
   */
  void removeFromPoller(shared_ptr<Channel> channel) {
    // shutDownWR(channel->getFd());
    poller_->epoll_del(channel);
  }
  
  /**
   * @brief 更新epoll中的通道事件
   * 
   * @param channel 要更新的通道
   * @param timeout 超时时间（毫秒），0表示不设置超时
   */
  void updatePoller(shared_ptr<Channel> channel, int timeout = 0) {
    poller_->epoll_mod(channel, timeout);
  }
  
  /**
   * @brief 将通道添加到epoll中
   * 
   * @param channel 要添加的通道
   * @param timeout 超时时间（毫秒），0表示不设置超时
   */
  void addToPoller(shared_ptr<Channel> channel, int timeout = 0) {
    poller_->epoll_add(channel, timeout);
  }

 private:
  // 声明顺序很重要：wakeupFd_ 必须在 pwakeupChannel_ 之前
  bool looping_;                    ///< 是否正在运行事件循环
  shared_ptr<Epoll> poller_;        ///< epoll实例，用于IO多路复用
  int wakeupFd_;                    ///< eventfd文件描述符，用于跨线程唤醒
  bool quit_;                       ///< 是否退出事件循环
  bool eventHandling_;              ///< 是否正在处理事件
  mutable MutexLock mutex_;         ///< 保护pendingFunctors_的互斥锁
  std::vector<Functor> pendingFunctors_;  ///< 待执行的回调函数队列
  bool callingPendingFunctors_;     ///< 是否正在执行待处理的回调函数
  const pid_t threadId_;           ///< 事件循环所属的线程ID
  shared_ptr<Channel> pwakeupChannel_;  ///< wakeupFd_对应的通道

  /**
   * @brief 唤醒事件循环（通过向wakeupFd_写入数据）
   * 
   * @details 用于跨线程唤醒阻塞在epoll_wait中的事件循环
   */
  void wakeup();
  
  /**
   * @brief 处理wakeupFd_的读事件
   * 
   * @details 读取wakeupFd_中的数据，清除唤醒信号
   */
  void handleRead();
  
  /**
   * @brief 执行待处理的回调函数
   * 
   * @details 将pendingFunctors_中的回调函数全部执行完
   */
  void doPendingFunctors();
  
  /**
   * @brief 处理wakeup通道的连接事件
   * 
   * @details 更新wakeup通道在epoll中的状态
   */
  void handleConn();
};
