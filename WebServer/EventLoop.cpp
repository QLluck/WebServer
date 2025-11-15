/// @file EventLoop.cpp
/// @brief 事件循环类实现文件
/// @author Lin Ya
/// @email xxbbb@vip.qq.com
/// 
/// @details 实现了EventLoop类的所有方法，包括事件循环主逻辑、
/// 跨线程任务执行、wakeup机制等

#include "EventLoop.h"
#include <sys/epoll.h>    ///< epoll相关定义
#include <sys/eventfd.h>  ///< eventfd系统调用（用于线程间通信）
#include <iostream>       ///< 输入输出流
#include "Util.h"         ///< 工具函数
#include "base/Logging.h" ///< 日志系统

using namespace std;

/// @brief 线程局部变量，存储当前线程的EventLoop指针
/// @details 用于实现One Loop Per Thread模式，确保每个线程只有一个EventLoop
__thread EventLoop* t_loopInThisThread = 0;

/**
 * @brief 创建eventfd文件描述符
 * 
 * @return int eventfd文件描述符
 * 
 * @details 创建非阻塞、执行时关闭的eventfd，用于跨线程唤醒
 * EFD_NONBLOCK: 非阻塞模式
 * EFD_CLOEXEC: 执行exec时自动关闭
 */
int createEventfd() {
  int evtfd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
  if (evtfd < 0) {
    LOG << "Failed in eventfd";
    abort();
  }
  return evtfd;
}

/**
 * @brief EventLoop构造函数，初始化事件循环
 * 
 * @details 初始化流程：
 * 1. 创建epoll实例
 * 2. 创建eventfd用于跨线程唤醒
 * 3. 创建wakeup通道并注册到epoll
 * 4. 设置线程局部变量，确保One Loop Per Thread
 */
EventLoop::EventLoop()
    : looping_(false),
      poller_(new Epoll()),
      wakeupFd_(createEventfd()),
      quit_(false),
      eventHandling_(false),
      callingPendingFunctors_(false),
      threadId_(CurrentThread::tid()),
      pwakeupChannel_(new Channel(this, wakeupFd_)) {
  // 检查当前线程是否已有EventLoop（One Loop Per Thread）
  if (t_loopInThisThread) {
    // LOG << "Another EventLoop " << t_loopInThisThread << " exists in this
    // thread " << threadId_;
  } else {
    t_loopInThisThread = this;
  }
  // 配置wakeup通道监听读事件（边缘触发模式）
  // pwakeupChannel_->setEvents(EPOLLIN | EPOLLET | EPOLLONESHOT);
  pwakeupChannel_->setEvents(EPOLLIN | EPOLLET);
  pwakeupChannel_->setReadHandler(bind(&EventLoop::handleRead, this));
  pwakeupChannel_->setConnHandler(bind(&EventLoop::handleConn, this));
  poller_->epoll_add(pwakeupChannel_, 0);
}

/**
 * @brief 处理wakeup通道的连接事件
 * 
 * @details 更新wakeup通道在epoll中的状态，确保继续监听
 */
void EventLoop::handleConn() {
  // poller_->epoll_mod(wakeupFd_, pwakeupChannel_, (EPOLLIN | EPOLLET |
  // EPOLLONESHOT), 0);
  updatePoller(pwakeupChannel_, 0);
}

/**
 * @brief EventLoop析构函数，清理资源
 * 
 * @details 关闭wakeupFd_，清空线程局部变量
 */
EventLoop::~EventLoop() {
  // wakeupChannel_->disableAll();
  // wakeupChannel_->remove();
  close(wakeupFd_);
  t_loopInThisThread = NULL;
}

/**
 * @brief 唤醒事件循环
 * 
 * @details 向wakeupFd_写入8字节数据，触发EPOLLIN事件，
 * 从而唤醒阻塞在epoll_wait中的事件循环线程
 */
void EventLoop::wakeup() {
  uint64_t one = 1;
  ssize_t n = writen(wakeupFd_, (char*)(&one), sizeof one);
  if (n != sizeof one) {
    LOG << "EventLoop::wakeup() writes " << n << " bytes instead of 8";
  }
}

/**
 * @brief 处理wakeupFd_的读事件
 * 
 * @details 读取wakeupFd_中的数据（8字节），清除唤醒信号
 * 重置wakeup通道的事件监听
 */
void EventLoop::handleRead() {
  uint64_t one = 1;
  ssize_t n = readn(wakeupFd_, &one, sizeof one);
  if (n != sizeof one) {
    LOG << "EventLoop::handleRead() reads " << n << " bytes instead of 8";
  }
  // pwakeupChannel_->setEvents(EPOLLIN | EPOLLET | EPOLLONESHOT);
  pwakeupChannel_->setEvents(EPOLLIN | EPOLLET);
}

/**
 * @brief 在当前线程中执行回调函数
 * 
 * @param cb 要执行的回调函数
 * 
 * @details 如果当前线程就是事件循环线程，直接执行回调；
 * 否则将回调加入队列，通过wakeup机制唤醒事件循环线程执行
 */
void EventLoop::runInLoop(Functor&& cb) {
  if (isInLoopThread())
    cb();
  else
    queueInLoop(std::move(cb));
}

/**
 * @brief 将回调函数加入待执行队列
 * 
 * @param cb 要执行的回调函数
 * 
 * @details 线程安全地将回调函数加入pendingFunctors_队列。
 * 如果当前不在事件循环线程，或正在执行待处理的回调函数，
 * 则唤醒事件循环线程，确保回调能及时执行
 */
void EventLoop::queueInLoop(Functor&& cb) {
  {
    MutexLockGuard lock(mutex_);
    pendingFunctors_.emplace_back(std::move(cb));
  }

  if (!isInLoopThread() || callingPendingFunctors_) wakeup();
}

/**
 * @brief 事件循环主函数（阻塞调用）
 * 
 * @details 主循环流程：
 * 1. 调用epoll_wait等待IO事件（最多等待10秒）
 * 2. 处理所有活跃的IO事件（调用Channel::handleEvents）
 * 3. 执行待处理的回调函数（跨线程任务）
 * 4. 处理超时的定时器
 * 循环直到quit_为true
 * 
 * @note 必须在事件循环所属的线程中调用
 */
void EventLoop::loop() {
  assert(!looping_);
  assert(isInLoopThread());
  looping_ = true;
  quit_ = false;
  // LOG_TRACE << "EventLoop " << this << " start looping";
  std::vector<SP_Channel> ret;
  while (!quit_) {
    // cout << "doing" << endl;
    ret.clear();
    ret = poller_->poll();  // 获取活跃事件
    eventHandling_ = true;
    for (auto& it : ret) it->handleEvents();  // 处理所有活跃事件
    eventHandling_ = false;
    doPendingFunctors();  // 执行待处理的回调函数
    poller_->handleExpired();  // 处理超时的定时器
  }
  looping_ = false;
}

/**
 * @brief 执行待处理的回调函数
 * 
 * @details 将pendingFunctors_中的回调函数全部执行完。
 * 使用swap减少锁持有时间，提高性能
 */
void EventLoop::doPendingFunctors() {
  std::vector<Functor> functors;
  callingPendingFunctors_ = true;

  {
    MutexLockGuard lock(mutex_);
    functors.swap(pendingFunctors_);  // 交换，减少锁持有时间
  }

  for (size_t i = 0; i < functors.size(); ++i) functors[i]();
  callingPendingFunctors_ = false;
}

/**
 * @brief 退出事件循环
 * 
 * @details 设置quit_标志为true，如果不在事件循环线程中则唤醒，
 * 确保事件循环能及时退出
 */
void EventLoop::quit() {
  quit_ = true;
  if (!isInLoopThread()) {
    wakeup();
  }
}