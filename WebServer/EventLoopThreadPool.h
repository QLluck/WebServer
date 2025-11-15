/// @file EventLoopThreadPool.h
/// @brief 事件循环线程池类头文件
/// @author Lin Ya
/// @email xxbbb@vip.qq.com
/// 
/// @details EventLoopThreadPool管理多个EventLoopThread，
/// 实现Round Robin负载均衡分配连接

#pragma once
#include <memory>              ///< 智能指针
#include <vector>              ///< 动态数组容器
#include "EventLoopThread.h"   ///< 事件循环线程类
#include "base/Logging.h"      ///< 日志系统
#include "base/noncopyable.h"  ///< 禁止拷贝基类

/**
 * @class EventLoopThreadPool
 * @brief 事件循环线程池类，管理多个工作线程（SubReactor）
 * 
 * @details 实现Round Robin负载均衡：
 * - 主线程（MainReactor）accept新连接
 * - 使用Round Robin方式将连接分配给工作线程
 * - 每个工作线程拥有独立的EventLoop
 */
class EventLoopThreadPool : noncopyable {
 public:
  /**
   * @brief 构造函数，初始化线程池
   * 
   * @param baseLoop 主事件循环（MainReactor）
   * @param numThreads 工作线程数量（SubReactor数量）
   * 
   * @details 如果numThreads <= 0，程序会abort
   */
  EventLoopThreadPool(EventLoop* baseLoop, int numThreads);

  /**
   * @brief 析构函数
   */
  ~EventLoopThreadPool() { LOG << "~EventLoopThreadPool()"; }
  
  /**
   * @brief 启动线程池，创建工作线程
   * 
   * @details 创建numThreads_个EventLoopThread并启动，
   * 收集所有EventLoop指针
   * 
   * @note 必须在主事件循环线程中调用
   */
  void start();

  /**
   * @brief 获取下一个EventLoop（Round Robin）
   * 
   * @return EventLoop* 下一个EventLoop指针
   * 
   * @details 使用Round Robin方式轮询分配，实现负载均衡
   * 如果线程池为空，返回主事件循环
   * 
   * @note 必须在主事件循环线程中调用
   */
  EventLoop* getNextLoop();

 private:
  EventLoop* baseLoop_;  ///< 主事件循环（MainReactor）
  bool started_;         ///< 是否已启动
  int numThreads_;       ///< 工作线程数量
  int next_;             ///< 下一个要分配的线程索引（Round Robin）
  std::vector<std::shared_ptr<EventLoopThread>> threads_;  ///< 工作线程列表
  std::vector<EventLoop*> loops_;  ///< 工作线程的EventLoop指针列表
};