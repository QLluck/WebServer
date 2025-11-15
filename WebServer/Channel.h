/// @file Channel.h
/// @brief 事件通道类头文件，封装文件描述符的IO事件和回调函数
/// @author Lin Ya
/// @email xxbbb@vip.qq.com
/// 
/// @details Channel类是Reactor模式中的核心组件，负责：
/// - 封装一个文件描述符的IO事件
/// - 管理事件回调函数（读、写、错误、连接）
/// - 与EventLoop和Epoll交互

#pragma once
#include <sys/epoll.h>    ///< epoll相关定义（EPOLLIN、EPOLLOUT等）
#include <functional>     ///< 函数对象和回调函数
#include <memory>         ///< 智能指针
#include <string>         ///< 字符串类
#include <unordered_map>  ///< 无序映射容器
#include "Timer.h"        ///< 定时器相关类

// 前向声明
class EventLoop;  ///< 事件循环类
class HttpData;   ///< HTTP请求处理类

/**
 * @class Channel
 * @brief 事件通道类，封装文件描述符的IO事件和回调函数
 * 
 * @details Channel是Reactor模式中的核心组件，每个文件描述符对应一个Channel对象
 * 负责管理该文件描述符关注的事件类型和对应的回调函数
 */
class Channel {
 private:
  typedef std::function<void()> CallBack;  ///< 回调函数类型定义
  
  EventLoop *loop_;        ///< 所属的事件循环
  int fd_;                 ///< 关联的文件描述符
  __uint32_t events_;      ///< 关注的事件类型（EPOLLIN、EPOLLOUT等）
  __uint32_t revents_;     ///< epoll返回的就绪事件
  __uint32_t lastEvents_;  ///< 上次关注的事件类型（用于比较是否需要更新epoll）

  // 方便找到上层持有该Channel的对象（使用weak_ptr避免循环引用）
  std::weak_ptr<HttpData> holder_;

  // 以下三个函数在Channel中未使用，实际在HttpData中实现
  int parse_URI();
  int parse_Headers();
  int analysisRequest();

  CallBack readHandler_;   ///< 读事件回调函数
  CallBack writeHandler_;  ///< 写事件回调函数
  CallBack errorHandler_; ///< 错误事件回调函数
  CallBack connHandler_;   ///< 连接事件回调函数

 public:
  /**
   * @brief 构造函数（不指定文件描述符）
   * 
   * @param loop 所属的事件循环
   */
  Channel(EventLoop *loop);
  
  /**
   * @brief 构造函数（指定文件描述符）
   * 
   * @param loop 所属的事件循环
   * @param fd 关联的文件描述符
   */
  Channel(EventLoop *loop, int fd);
  
  /**
   * @brief 析构函数
   */
  ~Channel();
  
  /**
   * @brief 获取文件描述符
   * 
   * @return int 文件描述符
   */
  int getFd();
  
  /**
   * @brief 设置文件描述符
   * 
   * @param fd 文件描述符
   */
  void setFd(int fd);

  /**
   * @brief 设置持有者（HttpData对象）
   * 
   * @param holder HttpData对象的智能指针
   */
  void setHolder(std::shared_ptr<HttpData> holder) { holder_ = holder; }
  
  /**
   * @brief 获取持有者（HttpData对象）
   * 
   * @return std::shared_ptr<HttpData> HttpData对象的智能指针
   */
  std::shared_ptr<HttpData> getHolder() {
    std::shared_ptr<HttpData> ret(holder_.lock());
    return ret;
  }

  /**
   * @brief 设置读事件回调函数
   * 
   * @param readHandler 读事件回调函数
   */
  void setReadHandler(CallBack &&readHandler) { readHandler_ = readHandler; }
  
  /**
   * @brief 设置写事件回调函数
   * 
   * @param writeHandler 写事件回调函数
   */
  void setWriteHandler(CallBack &&writeHandler) {
    writeHandler_ = writeHandler;
  }
  
  /**
   * @brief 设置错误事件回调函数
   * 
   * @param errorHandler 错误事件回调函数
   */
  void setErrorHandler(CallBack &&errorHandler) {
    errorHandler_ = errorHandler;
  }
  
  /**
   * @brief 设置连接事件回调函数
   * 
   * @param connHandler 连接事件回调函数
   */
  void setConnHandler(CallBack &&connHandler) { connHandler_ = connHandler; }

  /**
   * @brief 处理事件，根据revents_调用相应的回调函数
   * 
   * @details 事件处理顺序：
   * 1. 检查EPOLLHUP（对端关闭）
   * 2. 检查EPOLLERR（错误事件）
   * 3. 处理EPOLLIN/EPOLLPRI/EPOLLRDHUP（读事件）
   * 4. 处理EPOLLOUT（写事件）
   * 5. 处理连接事件
   */
  void handleEvents() {
    events_ = 0;
    if ((revents_ & EPOLLHUP) && !(revents_ & EPOLLIN)) {
      events_ = 0;
      return;
    }
    if (revents_ & EPOLLERR) {
      if (errorHandler_) errorHandler_();
      events_ = 0;
      return;
    }
    if (revents_ & (EPOLLIN | EPOLLPRI | EPOLLRDHUP)) {
      handleRead();
    }
    if (revents_ & EPOLLOUT) {
      handleWrite();
    }
    handleConn();
  }
  
  /**
   * @brief 处理读事件，调用读事件回调函数
   */
  void handleRead();
  
  /**
   * @brief 处理写事件，调用写事件回调函数
   */
  void handleWrite();
  
  /**
   * @brief 处理错误事件（未使用）
   */
  void handleError(int fd, int err_num, std::string short_msg);
  
  /**
   * @brief 处理连接事件，调用连接事件回调函数
   */
  void handleConn();

  /**
   * @brief 设置epoll返回的就绪事件
   * 
   * @param ev 就绪事件类型
   */
  void setRevents(__uint32_t ev) { revents_ = ev; }

  /**
   * @brief 设置关注的事件类型
   * 
   * @param ev 事件类型（EPOLLIN、EPOLLOUT等）
   */
  void setEvents(__uint32_t ev) { events_ = ev; }
  
  /**
   * @brief 获取关注的事件类型（可修改的引用）
   * 
   * @return __uint32_t& 事件类型的引用
   */
  __uint32_t &getEvents() { return events_; }

  /**
   * @brief 比较并更新上次关注的事件类型
   * 
   * @return bool 如果事件类型未改变返回true，否则返回false
   * 
   * @details 用于判断是否需要更新epoll中的事件注册
   */
  bool EqualAndUpdateLastEvents() {
    bool ret = (lastEvents_ == events_);
    lastEvents_ = events_;
    return ret;
  }

  /**
   * @brief 获取上次关注的事件类型
   * 
   * @return __uint32_t 上次的事件类型
   */
  __uint32_t getLastEvents() { return lastEvents_; }
};

typedef std::shared_ptr<Channel> SP_Channel;