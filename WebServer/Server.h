/// @file Server.h
/// @brief 服务器核心类头文件，定义了Server类及其接口
/// @author Lin Ya
/// @email xxbbb@vip.qq.com
/// 
/// @details Server类是Web服务器的核心类，采用Reactor模式实现：
/// - 主线程（MainReactor）负责accept新连接
/// - 工作线程（SubReactor）负责处理已建立的连接
/// - 使用Round Robin方式将新连接分配给工作线程

#pragma once  ///< 预处理指令，确保头文件只被编译一次，防止重复包含

#include <memory>  ///< 包含C++标准库智能指针组件，用于管理动态内存

#include "Channel.h"               ///< 引入事件通道类，用于封装IO事件和回调
#include "EventLoop.h"             ///< 引入事件循环类，提供事件驱动核心功能
#include "EventLoopThreadPool.h"   ///< 引入事件循环线程池类，用于管理多线程事件循环

/**
 * @brief 服务器核心类，负责管理连接、事件循环和线程池，处理客户端请求
 * 
 * @details 采用Reactor模式设计：
 * - 主事件循环（MainReactor）：只负责accept新连接
 * - 子事件循环（SubReactor）：处理已建立连接的IO事件
 * - 使用epoll边缘触发（ET）模式提高性能
 * - 支持多线程并发处理
 */
class Server {
 public:
  /**
   * @brief 构造函数，初始化服务器核心组件
   * 
   * @param loop 主事件循环指针，负责处理监听socket的事件
   * @param threadNum 事件循环线程池中的线程数量（工作线程数）
   * @param port 服务器监听的端口号
   * 
   * @details 构造函数会：
   * - 创建事件循环线程池
   * - 创建监听socket并绑定端口
   * - 创建accept通道用于监听新连接
   * - 设置socket为非阻塞模式
   */
  Server(EventLoop *loop, int threadNum, int port);

  /**
   * @brief 析构函数，默认实现
   * 
   * @note 智能指针会自动管理资源释放，无需手动处理
   */
  ~Server() {}

  /**
   * @brief 获取主事件循环指针
   * 
   * @return EventLoop* 指向主事件循环的指针
   */
  EventLoop *getLoop() const { return loop_; }

  /**
   * @brief 启动服务器
   * 
   * @details 启动流程：
   * - 启动事件循环线程池（创建工作线程）
   * - 配置accept通道监听EPOLLIN事件（边缘触发）
   * - 绑定新连接处理回调函数
   * - 将accept通道加入主事件循环的epoll
   * - 设置服务器启动状态为true
   */
  void start();

  /**
   * @brief 处理新客户端连接请求
   * 
   * @details 当监听socket触发EPOLLIN事件时被调用，主要工作：
   * - 循环accept所有待处理的连接（ET模式需要一次性处理完）
   * - 从线程池中获取一个事件循环（Round Robin分配）
   * - 设置新socket为非阻塞模式
   * - 创建HttpData对象处理该连接
   * - 将初始化任务加入对应事件循环的队列
   * 
   * @note 使用边缘触发模式，需要循环accept直到返回EAGAIN
   */
  void handNewConn();

  /**
   * @brief 更新监听通道在事件循环中的状态
   * 
   * @details 将acceptChannel_的状态更新到主事件循环的poller中，
   * 确保监听socket继续被监控，能够接收新的连接请求
   */
  void handThisConn() { loop_->updatePoller(acceptChannel_); }

 private:
  EventLoop *loop_;  ///< 主事件循环指针，负责监控监听socket和调度事件

  int threadNum_;  ///< 事件循环线程池的线程数量（工作线程数）

  ///< 事件循环线程池的智能指针，管理多个子事件循环（SubReactor）
  std::unique_ptr<EventLoopThreadPool> eventLoopThreadPool_;

  bool started_;  ///< 服务器启动状态标识（true：已启动，false：未启动）

  ///< 处理新连接事件的通道智能指针，关联监听socket的IO事件
  std::shared_ptr<Channel> acceptChannel_;

  int port_;  ///< 服务器监听的端口号

  int listenFd_;  ///< 监听socket的文件描述符，用于接收客户端连接请求

  static const int MAXFDS = 100000;  ///< 最大文件描述符数量限制，防止资源耗尽
};