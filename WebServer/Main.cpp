/// @file Main.cpp
/// @brief Web服务器程序入口文件，负责解析命令行参数、初始化服务器并启动事件循环
/// @author Lin Ya
/// @email xxbbb@vip.qq.com
/// 
/// @details 本文件是Web服务器的启动入口，主要功能包括：
/// - 解析命令行参数（线程数、端口号、日志路径）
/// - 初始化日志系统
/// - 创建主事件循环和服务器对象
/// - 启动服务器并进入事件循环

#include <getopt.h>   ///< 用于解析命令行参数
#include <string>     ///< 提供字符串处理功能

#include "EventLoop.h"    ///< 事件循环类，负责IO事件和定时器的调度
#include "Server.h"       ///< 服务器主类，管理连接和客户端请求
#include "base/Logging.h" ///< 日志工具类，提供日志输出功能

/**
 * @brief 程序主入口函数
 * 
 * @param argc 命令行参数个数
 * @param argv 命令行参数数组
 * @return int 程序退出码，0表示正常退出
 * 
 * @details 支持的命令行参数：
 * - `-t <num>`: 设置工作线程数量（默认4）
 * - `-p <port>`: 设置监听端口号（默认80）
 * - `-l <path>`: 设置日志文件路径，必须以'/'开头（默认./WebServer.log）
 * 
 * @example
 * ```bash
 * ./WebServer -t 8 -p 8080 -l /var/log/webserver.log
 * ```
 */
int main(int argc, char *argv[]) {
  int threadNum = 4;      ///< 工作线程数量，默认4个
  int port = 80;          ///< 监听端口号，默认80
  std::string logPath = "./WebServer.log";  ///< 日志文件路径，默认当前目录

  // 解析命令行参数：-t 线程数, -p 端口, -l 日志路径
  int opt;
  const char *str = "t:l:p:";  ///< getopt选项字符串：t和l和p都需要参数
  while ((opt = getopt(argc, argv, str)) != -1) {
    switch (opt) {
      case 't': {  ///< 设置线程数
        threadNum = atoi(optarg);
        break;
      }
      case 'l': {  ///< 设置日志路径
        logPath = optarg;
        // 日志路径必须以'/'开头（绝对路径）
        if (logPath.size() < 2 || optarg[0] != '/') {
          printf("logPath should start with \"/\"\n");
          abort();
        }
        break;
      }
      case 'p': {  ///< 设置端口号
        port = atoi(optarg);
        break;
      }
      default:
        break;
    }
  }
  
  // 初始化日志系统，设置日志文件路径
  Logger::setLogFileName(logPath);
  
  // 检查是否定义了_PTHREADS宏（多线程支持）
  // STL库在多线程上应用需要此宏定义
#ifndef _PTHREADS
  LOG << "_PTHREADS is not defined !";
#endif
  
  // 创建主事件循环，负责监听新连接和事件分发
  EventLoop mainLoop;
  
  // 创建HTTP服务器对象，传入主事件循环、线程数和端口号
  Server myHTTPServer(&mainLoop, threadNum, port);
  
  // 启动服务器（初始化线程池、配置监听通道等）
  myHTTPServer.start();
  
  // 进入事件循环，开始处理IO事件（阻塞调用）
  mainLoop.loop();
  
  return 0;
}
