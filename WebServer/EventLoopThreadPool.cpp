/// @file EventLoopThreadPool.cpp
/// @brief 事件循环线程池类实现文件
/// @author Lin Ya
/// @email xxbbb@vip.qq.com
/// 
/// @details 实现了EventLoopThreadPool类的所有方法

#include "EventLoopThreadPool.h"

/**
 * @brief EventLoopThreadPool构造函数
 * 
 * @param baseLoop 主事件循环（MainReactor）
 * @param numThreads 工作线程数量（SubReactor数量）
 * 
 * @details 初始化成员变量，检查numThreads是否有效
 */
EventLoopThreadPool::EventLoopThreadPool(EventLoop *baseLoop, int numThreads)
    : baseLoop_(baseLoop), started_(false), numThreads_(numThreads), next_(0) {
  if (numThreads_ <= 0) {
    LOG << "numThreads_ <= 0";
    abort();
  }
}

/**
 * @brief 启动线程池，创建工作线程
 * 
 * @details 执行流程：
 * 1. 检查是否在主事件循环线程中
 * 2. 创建numThreads_个EventLoopThread
 * 3. 启动每个线程并获取其EventLoop指针
 * 4. 将EventLoop指针存入loops_数组
 * 
 * @note 必须在主事件循环线程中调用
 */
void EventLoopThreadPool::start() {
  baseLoop_->assertInLoopThread();
  started_ = true;
  for (int i = 0; i < numThreads_; ++i) {
    std::shared_ptr<EventLoopThread> t(new EventLoopThread());
    threads_.push_back(t);
    loops_.push_back(t->startLoop());  // 启动线程并获取EventLoop指针
  }
}

/**
 * @brief 获取下一个EventLoop（Round Robin负载均衡）
 * 
 * @return EventLoop* 下一个EventLoop指针
 * 
 * @details Round Robin算法：
 * 1. 如果线程池为空，返回主事件循环
 * 2. 否则返回loops_[next_]
 * 3. next_ = (next_ + 1) % numThreads_，实现轮询
 * 
 * @note 必须在主事件循环线程中调用
 */
EventLoop *EventLoopThreadPool::getNextLoop() {
  baseLoop_->assertInLoopThread();
  assert(started_);
  EventLoop *loop = baseLoop_;
  if (!loops_.empty()) {
    loop = loops_[next_];
    next_ = (next_ + 1) % numThreads_;  // Round Robin轮询
  }
  return loop;
}