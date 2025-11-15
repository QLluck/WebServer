/// @file Timer.cpp
/// @brief 定时器类实现文件
/// @author Lin Ya
/// @email xxbbb@vip.qq.com
/// 
/// @details 实现了TimerNode和TimerManager的所有方法

#include "Timer.h"
#include <sys/time.h>  ///< 时间相关结构体和函数
#include <unistd.h>    ///< POSIX标准定义
#include <queue>       ///< 优先队列

/**
 * @brief TimerNode构造函数，创建定时器节点
 * 
 * @param requestData 关联的HttpData对象
 * @param timeout 超时时间（毫秒）
 * 
 * @details 计算过期时间 = 当前时间（毫秒） + timeout
 * 使用gettimeofday获取当前时间，转换为毫秒
 */
TimerNode::TimerNode(std::shared_ptr<HttpData> requestData, int timeout)
    : deleted_(false), SPHttpData(requestData) {
  struct timeval now;
  gettimeofday(&now, NULL);
  // 以毫秒计：秒数取模10000（避免溢出）* 1000 + 微秒数 / 1000
  expiredTime_ =
      (((now.tv_sec % 10000) * 1000) + (now.tv_usec / 1000)) + timeout;
}

/**
 * @brief TimerNode析构函数，关闭关联的连接
 * 
 * @details 如果HttpData还存在，调用handleClose关闭连接
 */
TimerNode::~TimerNode() {
  if (SPHttpData) SPHttpData->handleClose();
}

/**
 * @brief TimerNode拷贝构造函数
 */
TimerNode::TimerNode(TimerNode &tn)
    : SPHttpData(tn.SPHttpData), expiredTime_(0) {}

/**
 * @brief 更新定时器的超时时间
 * 
 * @param timeout 新的超时时间（毫秒）
 * 
 * @details 重新计算过期时间 = 当前时间 + timeout
 */
void TimerNode::update(int timeout) {
  struct timeval now;
  gettimeofday(&now, NULL);
  expiredTime_ =
      (((now.tv_sec % 10000) * 1000) + (now.tv_usec / 1000)) + timeout;
}

/**
 * @brief 检查定时器是否有效（未超时）
 * 
 * @return bool 如果当前时间小于过期时间返回true，否则返回false并标记为deleted
 * 
 * @details 比较当前时间和过期时间，如果超时则标记为deleted
 */
bool TimerNode::isValid() {
  struct timeval now;
  gettimeofday(&now, NULL);
  size_t temp = (((now.tv_sec % 10000) * 1000) + (now.tv_usec / 1000));
  if (temp < expiredTime_)
    return true;
  else {
    this->setDeleted();
    return false;
  }
}

/**
 * @brief 清除关联的HttpData，标记为deleted
 * 
 * @details 释放HttpData的智能指针，标记定时器为已删除
 * 用于延迟删除策略
 */
void TimerNode::clearReq() {
  SPHttpData.reset();
  this->setDeleted();
}

/**
 * @brief TimerManager构造函数
 */
TimerManager::TimerManager() {}

/**
 * @brief TimerManager析构函数
 */
TimerManager::~TimerManager() {}

/**
 * @brief 添加定时器
 * 
 * @param SPHttpData HttpData对象的智能指针
 * @param timeout 超时时间（毫秒）
 * 
 * @details 创建TimerNode并加入优先队列，同时将TimerNode关联到HttpData
 * 这样HttpData可以访问自己的定时器，实现双向关联
 */
void TimerManager::addTimer(std::shared_ptr<HttpData> SPHttpData, int timeout) {
  SPTimerNode new_node(new TimerNode(SPHttpData, timeout));
  timerNodeQueue.push(new_node);
  SPHttpData->linkTimer(new_node);
}

/**
 * @brief 处理超时的定时器
 * 
 * @details 延迟删除策略说明：
 * 
 * 因为(1) 优先队列不支持随机访问
 * (2) 即使支持，随机删除某节点后破坏了堆的结构，需要重新更新堆结构。
 * 
 * 所以对于被置为deleted的时间节点，会延迟到它(1)超时 或
 * (2)它前面的节点都被删除时，它才会被删除。
 * 一个点被置为deleted,它最迟会在TIMER_TIME_OUT时间后被删除。
 * 
 * 这样做有两个好处：
 * (1) 第一个好处是不需要遍历优先队列，省时。
 * (2) 第二个好处是给超时时间一个容忍的时间，就是设定的超时时间是删除的下限
 * (并不是一到超时时间就立即删除)，如果监听的请求在超时后的下一次请求中又一次出现了，
 * 就不用再重新申请RequestData节点了，这样可以继续重复利用前面的RequestData，
 * 减少了一次delete和一次new的时间。
 * 
 * 处理流程：
 * 从优先队列顶部开始，删除所有已删除（deleted）或已超时（!isValid）的定时器
 * 直到遇到第一个有效的定时器为止
 */
void TimerManager::handleExpiredEvent() {
  // MutexLockGuard locker(lock);  // 当前未使用锁，因为只在EventLoop线程中调用
  while (!timerNodeQueue.empty()) {
    SPTimerNode ptimer_now = timerNodeQueue.top();
    if (ptimer_now->isDeleted())  // 已标记为删除
      timerNodeQueue.pop();
    else if (ptimer_now->isValid() == false)  // 已超时
      timerNodeQueue.pop();
    else
      break;  // 遇到有效的定时器，停止删除
  }
}