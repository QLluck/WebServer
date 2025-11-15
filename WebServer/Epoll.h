/// @file Epoll.h
/// @brief epoll封装类头文件，封装Linux epoll系统调用
/// @author Lin Ya
/// @email xxbbb@vip.qq.com
/// 
/// @details Epoll类封装了epoll的创建、添加、修改、删除、等待等操作
/// 提供文件描述符到Channel和HttpData的映射，管理定时器

#pragma once
#include <sys/epoll.h>    ///< epoll相关系统调用和数据结构
#include <memory>          ///< 智能指针
#include <unordered_map>   ///< 无序映射容器
#include <vector>          ///< 动态数组容器
#include "Channel.h"      ///< 事件通道类
#include "HttpData.h"      ///< HTTP请求处理类
#include "Timer.h"        ///< 定时器相关类

/**
 * @class Epoll
 * @brief epoll封装类，封装Linux epoll系统调用
 * 
 * @details 提供epoll的完整封装，包括：
 * - 文件描述符的注册、修改、删除
 * - epoll_wait等待IO事件
 * - 文件描述符到Channel和HttpData的映射
 * - 定时器管理
 */
class Epoll {
 public:
  /**
   * @brief 构造函数，创建epoll实例
   * 
   * @details 使用epoll_create1创建epoll文件描述符，初始化事件数组
   */
  Epoll();
  
  /**
   * @brief 析构函数
   */
  ~Epoll();
  
  /**
   * @brief 向epoll中添加文件描述符
   * 
   * @param request 要添加的Channel对象
   * @param timeout 超时时间（毫秒），0表示不设置超时
   * 
   * @details 将Channel关联的文件描述符添加到epoll中监听，
   * 如果设置了超时时间，则同时添加定时器
   */
  void epoll_add(SP_Channel request, int timeout);
  
  /**
   * @brief 修改epoll中文件描述符的监听事件
   * 
   * @param request 要修改的Channel对象
   * @param timeout 超时时间（毫秒），0表示不设置超时
   * 
   * @details 更新Channel在epoll中的事件监听，如果事件类型未改变则跳过
   */
  void epoll_mod(SP_Channel request, int timeout);
  
  /**
   * @brief 从epoll中删除文件描述符
   * 
   * @param request 要删除的Channel对象
   * 
   * @details 从epoll中移除文件描述符，清除相关映射
   */
  void epoll_del(SP_Channel request);
  
  /**
   * @brief 等待IO事件（阻塞调用）
   * 
   * @return std::vector<std::shared_ptr<Channel>> 活跃事件的Channel列表
   * 
   * @details 调用epoll_wait等待IO事件，最多等待10秒
   * 如果没有任何事件，继续等待直到有事件发生
   */
  std::vector<std::shared_ptr<Channel>> poll();
  
  /**
   * @brief 处理epoll_wait返回的活跃事件
   * 
   * @param events_num epoll_wait返回的事件数量
   * @return std::vector<std::shared_ptr<Channel>> 活跃事件的Channel列表
   * 
   * @details 将epoll返回的事件转换为Channel对象列表
   */
  std::vector<std::shared_ptr<Channel>> getEventsRequest(int events_num);
  
  /**
   * @brief 为Channel添加定时器
   * 
   * @param request_data Channel对象
   * @param timeout 超时时间（毫秒）
   * 
   * @details 如果Channel关联了HttpData，则为其添加定时器
   */
  void add_timer(std::shared_ptr<Channel> request_data, int timeout);
  
  /**
   * @brief 获取epoll文件描述符
   * 
   * @return int epoll文件描述符
   */
  int getEpollFd() { return epollFd_; }
  
  /**
   * @brief 处理超时的定时器
   * 
   * @details 调用TimerManager处理所有超时的连接
   */
  void handleExpired();

 private:
  static const int MAXFDS = 100000;  ///< 最大文件描述符数量限制
  int epollFd_;                       ///< epoll文件描述符
  std::vector<epoll_event> events_;  ///< epoll_wait返回的事件数组
  std::shared_ptr<Channel> fd2chan_[MAXFDS];  ///< 文件描述符到Channel的映射
  std::shared_ptr<HttpData> fd2http_[MAXFDS]; ///< 文件描述符到HttpData的映射
  TimerManager timerManager_;        ///< 定时器管理器
};