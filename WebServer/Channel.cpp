/// @file Channel.cpp
/// @brief 事件通道类实现文件
/// @author Lin Ya
/// @email xxbbb@vip.qq.com
/// 
/// @details 实现了Channel类的基本方法，包括事件处理回调

#include "Channel.h"

#include <unistd.h>    ///< POSIX标准定义
#include <cstdlib>     ///< 标准库函数
#include <iostream>    ///< 输入输出流
#include <queue>       ///< 队列容器

#include "Epoll.h"      ///< epoll封装类
#include "EventLoop.h"  ///< 事件循环类
#include "Util.h"      ///< 工具函数

using namespace std;

/**
 * @brief 构造函数（不指定文件描述符）
 * 
 * @param loop 所属的事件循环
 */
Channel::Channel(EventLoop *loop)
    : loop_(loop), events_(0), lastEvents_(0), fd_(0) {}

/**
 * @brief 构造函数（指定文件描述符）
 * 
 * @param loop 所属的事件循环
 * @param fd 关联的文件描述符
 */
Channel::Channel(EventLoop *loop, int fd)
    : loop_(loop), fd_(fd), events_(0), lastEvents_(0) {}

/**
 * @brief 析构函数
 * 
 * @note 文件描述符的关闭由HttpData负责，Channel不负责关闭
 */
Channel::~Channel() {
  // loop_->poller_->epoll_del(fd, events_);
  // close(fd_);
}

/**
 * @brief 获取文件描述符
 * 
 * @return int 文件描述符
 */
int Channel::getFd() { return fd_; }

/**
 * @brief 设置文件描述符
 * 
 * @param fd 文件描述符
 */
void Channel::setFd(int fd) { fd_ = fd; }

/**
 * @brief 处理读事件，调用读事件回调函数
 * 
 * @details 如果设置了readHandler_，则调用它处理读事件
 */
void Channel::handleRead() {
  if (readHandler_) {
    readHandler_();
  }
}

/**
 * @brief 处理写事件，调用写事件回调函数
 * 
 * @details 如果设置了writeHandler_，则调用它处理写事件
 */
void Channel::handleWrite() {
  if (writeHandler_) {
    writeHandler_();
  }
}

/**
 * @brief 处理连接事件，调用连接事件回调函数
 * 
 * @details 如果设置了connHandler_，则调用它处理连接事件
 * 通常用于更新epoll中的事件注册
 */
void Channel::handleConn() {
  if (connHandler_) {
    connHandler_();
  }
}