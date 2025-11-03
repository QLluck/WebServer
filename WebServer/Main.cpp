/// @file main.cpp
/// @brief 服务器程序入口文件，负责解析命令参数、初始化服务器并启动事件循环
/// @author Lin Ya
/// @em xxbbb@vip.qq.com

#include <getopt.h>   ///< 用于解析命令行参数
#include <string>     ///< 提供字符串处理功能

#include "EventLoop.h"    ///< 事件循环类，负责IO事件和定时器的调度
#include "Server.h"       ///< 服务器主类，管理连接和客户端请求
#include "base/Logging.h" ///< 日志工具类，提供日志输出功能


int main(int argc, char *argv[]) {
  int threadNum = 4;
  int port = 80;
  std::string logPath = "./WebServer.log";

  // parse args
  int opt;
  const char *str = "t:l:p:";
  while ((opt = getopt(argc, argv, str)) != -1) {
    switch (opt) {
      case 't': {
        threadNum = atoi(optarg);
        break;
      }
      case 'l': {
        logPath = optarg;
        if (logPath.size() < 2 || optarg[0] != '/') {
          printf("logPath should start with \"/\"\n");
          abort();
        }
        break;
      }
      case 'p': {
        port = atoi(optarg);
        break;
      }
      default:
        break;
    }
  }
  Logger::setLogFileName(logPath);
// STL库在多线程上应用
#ifndef _PTHREADS
  LOG << "_PTHREADS is not defined !";
#endif
  EventLoop mainLoop;//事件监听
  Server myHTTPServer(&mainLoop, threadNum, port);//服务器对象
  myHTTPServer.start();
  mainLoop.loop();
  return 0;
}
