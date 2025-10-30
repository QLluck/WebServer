// @Author Lin Ya
// @Email xxbbb@vip.qq.com
#pragma once
#include <memory>
#include "Channel.h"
#include "EventLoop.h"
#include "EventLoopThreadPool.h"

class Server {
 public:
  Server(EventLoop *loop, int threadNum, int port);
  ~Server() {}
  EventLoop *getLoop() const { return loop_; }
  void start();
  void handNewConn();
  void handThisConn() { loop_->updatePoller(acceptChannel_); }

 private:
  EventLoop *loop_;//监听
  int threadNum_;//线程数
  std::unique_ptr<EventLoopThreadPool> eventLoopThreadPool_;//线程池指针
  bool started_;//开始状态
  std::shared_ptr<Channel> acceptChannel_;//管道指针
  int port_;//端口号
  int listenFd_;//
  static const int MAXFDS = 100000;
};