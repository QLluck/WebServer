/// @file Server.h
/// @brief 服务器核心类头文件，定义了Server类及其接口
/// @author Lin Ya
/// @email xxbbb@vip.qq.com

#pragma once  ///< 预处理指令，确保头文件只被编译一次，防止重复包含

#include <memory>  ///< 包含C++标准库智能指针组件，用于管理动态内存

#include "Channel.h"               ///< 引入事件通道类，用于封装IO事件和回调
#include "EventLoop.h"             ///< 引入事件循环类，提供事件驱动核心功能
#include "EventLoopThreadPool.h"   ///< 引入事件循环线程池类，用于管理多线程事件循环

/// @brief 服务器核心类，负责管理连接、事件循环和线程池，处理客户端请求
class Server {
 public:
  /// @brief 构造函数，初始化服务器核心组件
  /// @param loop 主事件循环指针，负责处理监听事件
  /// @param threadNum 事件循环线程池中的线程数量
  /// @param port 服务器监听的端口号
  Server(EventLoop *loop, int threadNum, int port);

  /// @brief 析构函数，默认实现（可根据需要添加资源释放逻辑）
  ~Server() {}

  /// @brief 获取主事件循环指针
  /// @return 指向主事件循环的指针
  EventLoop *getLoop() const { return loop_; }

  
  void start();

  /// @brief 处理新客户端连接请求
  /// @note 通常在有新连接事件触发时被回调，负责调用accept接收连接
  void handNewConn();

  /// @brief 更新监听通道在事件循环中的状态
  /// @note 将acceptChannel_的状态更新到主事件循环的poller中，使其能被监控
  void handThisConn() { loop_->updatePoller(acceptChannel_); }

 private:
  EventLoop *loop_;///< 主事件循环指针，负责监控监听socket和调度事件

  int threadNum_;///< 事件循环线程池的线程数量

  ///< 事件循环线程池的智能指针，管理多个子事件循环
  std::unique_ptr<EventLoopThreadPool> eventLoopThreadPool_;

  bool started_;///< 服务器启动状态标识（true：已启动，false：未启动）

  ///< 处理新连接事件的通道智能指针，关联监听socket的事件
  std::shared_ptr<Channel> acceptChannel_;

  int port_;///< 服务器监听的端口号

  int listenFd_;///< 监听socket的文件描述符，用于接收客户端连接请求

  static const int MAXFDS = 100000;///< 最大文件描述符数量限制
};