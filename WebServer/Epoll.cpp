/// @file Epoll.cpp
/// @brief epoll封装类实现文件
/// @author Lin Ya
/// @email xxbbb@vip.qq.com
/// 
/// @details 实现了Epoll类的所有方法，封装epoll系统调用

#include "Epoll.h"
#include <assert.h>        ///< 断言
#include <errno.h>         ///< 错误码
#include <netinet/in.h>    ///< 网络地址结构
#include <string.h>        ///< 字符串操作
#include <sys/epoll.h>     ///< epoll系统调用
#include <sys/socket.h>    ///< socket系统调用
#include <deque>           ///< 双端队列
#include <queue>           ///< 队列
#include "Util.h"          ///< 工具函数
#include "base/Logging.h"  ///< 日志系统

#include <arpa/inet.h>     ///< 网络地址转换
#include <iostream>
using namespace std;

const int EVENTSNUM = 4096;      ///< epoll_wait返回的最大事件数
const int EPOLLWAIT_TIME = 10000; ///< epoll_wait超时时间（毫秒）

typedef shared_ptr<Channel> SP_Channel;

/**
 * @brief Epoll构造函数，创建epoll实例
 * 
 * @details 使用epoll_create1创建epoll文件描述符（EPOLL_CLOEXEC标志），
 * 初始化事件数组大小为EVENTSNUM
 */
Epoll::Epoll() : epollFd_(epoll_create1(EPOLL_CLOEXEC)), events_(EVENTSNUM) {
  assert(epollFd_ > 0);
}

/**
 * @brief Epoll析构函数
 */
Epoll::~Epoll() {}

/**
 * @brief 向epoll中添加文件描述符
 * 
 * @param request 要添加的Channel对象
 * @param timeout 超时时间（毫秒），0表示不设置超时
 * 
 * @details 执行流程：
 * 1. 如果设置了超时时间，添加定时器
 * 2. 创建epoll_event结构，设置文件描述符和事件类型
 * 3. 更新Channel的lastEvents_
 * 4. 建立文件描述符到Channel的映射
 * 5. 调用epoll_ctl添加到epoll中
 */
void Epoll::epoll_add(SP_Channel request, int timeout) {
  int fd = request->getFd();
  if (timeout > 0) {
    add_timer(request, timeout);
    fd2http_[fd] = request->getHolder();
  }
  struct epoll_event event;
  event.data.fd = fd;
  event.events = request->getEvents();

  request->EqualAndUpdateLastEvents();

  fd2chan_[fd] = request;
  if (epoll_ctl(epollFd_, EPOLL_CTL_ADD, fd, &event) < 0) {
    perror("epoll_add error");
    fd2chan_[fd].reset();
  }
}

/**
 * @brief 修改epoll中文件描述符的监听事件
 * 
 * @param request 要修改的Channel对象
 * @param timeout 超时时间（毫秒），0表示不设置超时
 * 
 * @details 执行流程：
 * 1. 如果设置了超时时间，更新定时器
 * 2. 比较当前事件和上次事件，如果未改变则跳过
 * 3. 调用epoll_ctl修改epoll中的事件注册
 */
void Epoll::epoll_mod(SP_Channel request, int timeout) {
  if (timeout > 0) add_timer(request, timeout);
  int fd = request->getFd();
  if (!request->EqualAndUpdateLastEvents()) {
    struct epoll_event event;
    event.data.fd = fd;
    event.events = request->getEvents();
    if (epoll_ctl(epollFd_, EPOLL_CTL_MOD, fd, &event) < 0) {
      perror("epoll_mod error");
      fd2chan_[fd].reset();
    }
  }
}

/**
 * @brief 从epoll中删除文件描述符
 * 
 * @param request 要删除的Channel对象
 * 
 * @details 执行流程：
 * 1. 调用epoll_ctl从epoll中删除文件描述符
 * 2. 清除文件描述符到Channel和HttpData的映射
 */
void Epoll::epoll_del(SP_Channel request) {
  int fd = request->getFd();
  struct epoll_event event;
  event.data.fd = fd;
  event.events = request->getLastEvents();
  // event.events = 0;
  // request->EqualAndUpdateLastEvents()
  if (epoll_ctl(epollFd_, EPOLL_CTL_DEL, fd, &event) < 0) {
    perror("epoll_del error");
  }
  fd2chan_[fd].reset();
  fd2http_[fd].reset();
}

/**
 * @brief 等待IO事件（阻塞调用）
 * 
 * @return std::vector<SP_Channel> 活跃事件的Channel列表
 * 
 * @details 调用epoll_wait等待IO事件，最多等待EPOLLWAIT_TIME毫秒
 * 如果没有事件返回，继续等待直到有事件发生
 */
std::vector<SP_Channel> Epoll::poll() {
  while (true) {
    int event_count =
        epoll_wait(epollFd_, &*events_.begin(), events_.size(), EPOLLWAIT_TIME);
    if (event_count < 0) perror("epoll wait error");
    std::vector<SP_Channel> req_data = getEventsRequest(event_count);
    if (req_data.size() > 0) return req_data;
  }
}

/**
 * @brief 处理超时的定时器
 * 
 * @details 调用TimerManager处理所有超时的连接，关闭超时的HttpData
 */
void Epoll::handleExpired() { timerManager_.handleExpiredEvent(); }

/**
 * @brief 处理epoll_wait返回的活跃事件
 * 
 * @param events_num epoll_wait返回的事件数量
 * @return std::vector<SP_Channel> 活跃事件的Channel列表
 * 
 * @details 遍历所有活跃事件，通过文件描述符查找对应的Channel，
 * 设置Channel的就绪事件，返回Channel列表供EventLoop处理
 */
std::vector<SP_Channel> Epoll::getEventsRequest(int events_num) {
  std::vector<SP_Channel> req_data;
  for (int i = 0; i < events_num; ++i) {
    // 获取有事件产生的描述符
    int fd = events_[i].data.fd;

    SP_Channel cur_req = fd2chan_[fd];

    if (cur_req) {
      cur_req->setRevents(events_[i].events);  // 设置就绪事件
      cur_req->setEvents(0);  // 清空关注的事件（避免重复处理）
      // 加入线程池之前将Timer和request分离
      // cur_req->seperateTimer();
      req_data.push_back(cur_req);
    } else {
      LOG << "SP cur_req is invalid";
    }
  }
  return req_data;
}

/**
 * @brief 为Channel添加定时器
 * 
 * @param request_data Channel对象
 * @param timeout 超时时间（毫秒）
 * 
 * @details 如果Channel关联了HttpData，则为其添加定时器
 * 定时器超时后会自动关闭连接
 */
void Epoll::add_timer(SP_Channel request_data, int timeout) {
  shared_ptr<HttpData> t = request_data->getHolder();
  if (t)
    timerManager_.addTimer(t, timeout);
  else
    LOG << "timer add fail";
}