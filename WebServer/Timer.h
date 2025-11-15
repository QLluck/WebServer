/// @file Timer.h
/// @brief 定时器类头文件，用于管理HTTP连接的超时
/// @author Lin Ya
/// @email xxbbb@vip.qq.com
/// 
/// @details 使用小根堆（优先队列）实现定时器，自动关闭超时的连接

#pragma once
#include <unistd.h>          ///< POSIX标准定义
#include <deque>             ///< 双端队列
#include <memory>            ///< 智能指针
#include <queue>             ///< 队列和优先队列
#include "HttpData.h"        ///< HTTP请求处理类
#include "base/MutexLock.h"  ///< 互斥锁
#include "base/noncopyable.h" ///< 禁止拷贝基类

// 前向声明
class HttpData;

/**
 * @class TimerNode
 * @brief 定时器节点类，表示一个HTTP连接的定时器
 * 
 * @details 每个TimerNode关联一个HttpData对象，记录过期时间
 * 使用延迟删除策略，提高性能
 */
class TimerNode {
 public:
  /**
   * @brief 构造函数，创建定时器节点
   * 
   * @param requestData 关联的HttpData对象
   * @param timeout 超时时间（毫秒）
   * 
   * @details 计算过期时间 = 当前时间 + timeout
   */
  TimerNode(std::shared_ptr<HttpData> requestData, int timeout);
  
  /**
   * @brief 析构函数，关闭关联的HttpData连接
   */
  ~TimerNode();
  
  /**
   * @brief 拷贝构造函数
   */
  TimerNode(TimerNode &tn);
  
  /**
   * @brief 更新超时时间
   * 
   * @param timeout 新的超时时间（毫秒）
   */
  void update(int timeout);
  
  /**
   * @brief 检查定时器是否有效（未超时）
   * 
   * @return bool 如果未超时返回true，否则返回false并标记为deleted
   */
  bool isValid();
  
  /**
   * @brief 清除关联的HttpData，标记为deleted
   */
  void clearReq();
  
  /**
   * @brief 标记定时器为已删除
   */
  void setDeleted() { deleted_ = true; }
  
  /**
   * @brief 检查定时器是否已删除
   * 
   * @return bool 如果已删除返回true
   */
  bool isDeleted() const { return deleted_; }
  
  /**
   * @brief 获取过期时间
   * 
   * @return size_t 过期时间（毫秒时间戳）
   */
  size_t getExpTime() const { return expiredTime_; }

 private:
  bool deleted_;                    ///< 是否已删除（延迟删除标志）
  size_t expiredTime_;              ///< 过期时间（毫秒时间戳）
  std::shared_ptr<HttpData> SPHttpData;  ///< 关联的HttpData对象
};

/**
 * @struct TimerCmp
 * @brief 定时器比较函数，用于优先队列（小根堆）
 * 
 * @details 过期时间小的排在前面，优先被处理
 */
struct TimerCmp {
  /**
   * @brief 比较两个定时器节点
   * 
   * @param a 第一个定时器节点
   * @param b 第二个定时器节点
   * @return bool 如果a的过期时间大于b返回true（小根堆）
   */
  bool operator()(std::shared_ptr<TimerNode> &a,
                  std::shared_ptr<TimerNode> &b) const {
    return a->getExpTime() > b->getExpTime();
  }
};

/**
 * @class TimerManager
 * @brief 定时器管理器，使用优先队列管理所有定时器
 * 
 * @details 使用小根堆实现，自动处理超时的连接
 * 采用延迟删除策略，提高性能
 */
class TimerManager {
 public:
  /**
   * @brief 构造函数
   */
  TimerManager();
  
  /**
   * @brief 析构函数
   */
  ~TimerManager();
  
  /**
   * @brief 添加定时器
   * 
   * @param SPHttpData HttpData对象的智能指针
   * @param timeout 超时时间（毫秒）
   * 
   * @details 创建TimerNode并加入优先队列，同时关联到HttpData
   */
  void addTimer(std::shared_ptr<HttpData> SPHttpData, int timeout);
  
  /**
   * @brief 处理超时的定时器
   * 
   * @details 从优先队列顶部开始，删除所有已删除或已超时的定时器
   * 采用延迟删除策略，不需要遍历整个队列
   */
  void handleExpiredEvent();

 private:
  typedef std::shared_ptr<TimerNode> SPTimerNode;  ///< 定时器节点智能指针类型
  ///< 优先队列（小根堆），按过期时间排序
  std::priority_queue<SPTimerNode, std::deque<SPTimerNode>, TimerCmp>
      timerNodeQueue;
  // MutexLock lock;  ///< 互斥锁（当前未使用，因为只在EventLoop线程中调用）
};