/// @file HttpData.h
/// @brief HTTP请求处理类头文件，定义了HttpData类及其相关枚举类型
/// @author Lin Ya
/// @email xxbbb@vip.qq.com
/// 
/// @details 本文件定义了HTTP请求处理的核心类HttpData，包括：
/// - HTTP请求解析状态机
/// - HTTP方法、版本、连接状态等枚举
/// - MIME类型管理
/// - HTTP请求和响应的处理逻辑

#pragma once
#include <sys/epoll.h>    ///< epoll相关定义（EPOLLIN、EPOLLOUT等）
#include <unistd.h>       ///< POSIX标准定义（close等）
#include <functional>     ///< 函数对象和bind
#include <map>            ///< 有序映射容器（用于存储HTTP头）
#include <memory>         ///< 智能指针
#include <string>         ///< 字符串类
#include <unordered_map>  ///< 无序映射容器（用于MIME类型映射）
#include "Timer.h"        ///< 定时器相关类

// 前向声明
class EventLoop;   ///< 事件循环类
class TimerNode;   ///< 定时器节点类
class Channel;     ///< 事件通道类

/**
 * @enum ProcessState
 * @brief HTTP请求处理状态枚举
 * 
 * @details 表示HTTP请求解析和处理的各个阶段
 */
enum ProcessState {
  STATE_PARSE_URI = 1,      ///< 解析URI阶段
  STATE_PARSE_HEADERS,      ///< 解析HTTP头阶段
  STATE_RECV_BODY,          ///< 接收请求体阶段（POST请求）
  STATE_ANALYSIS,           ///< 分析请求并生成响应阶段
  STATE_FINISH              ///< 处理完成阶段
};

/**
 * @enum URIState
 * @brief URI解析结果状态枚举
 */
enum URIState {
  PARSE_URI_AGAIN = 1,      ///< 需要继续读取数据才能完成解析
  PARSE_URI_ERROR,          ///< URI解析错误
  PARSE_URI_SUCCESS,        ///< URI解析成功
};

/**
 * @enum HeaderState
 * @brief HTTP头解析结果状态枚举
 */
enum HeaderState {
  PARSE_HEADER_SUCCESS = 1, ///< HTTP头解析成功
  PARSE_HEADER_AGAIN,        ///< 需要继续读取数据才能完成解析
  PARSE_HEADER_ERROR         ///< HTTP头解析错误
};

/**
 * @enum AnalysisState
 * @brief 请求分析结果状态枚举
 */
enum AnalysisState { 
  ANALYSIS_SUCCESS = 1,  ///< 分析成功，已生成响应
  ANALYSIS_ERROR         ///< 分析失败（如文件不存在）
};

/**
 * @enum ParseState
 * @brief HTTP头解析状态机状态枚举
 * 
 * @details 用于状态机解析HTTP头部的每一行
 */
enum ParseState {
  H_START = 0,              ///< 开始状态
  H_KEY,                    ///< 解析键（header name）
  H_COLON,                  ///< 遇到冒号
  H_SPACES_AFTER_COLON,     ///< 冒号后的空格
  H_VALUE,                   ///< 解析值（header value）
  H_CR,                     ///< 回车符
  H_LF,                     ///< 换行符
  H_END_CR,                 ///< 头部结束的回车符
  H_END_LF                  ///< 头部结束的换行符（空行）
};

/**
 * @enum ConnectionState
 * @brief 连接状态枚举
 */
enum ConnectionState { 
  H_CONNECTED = 0,    ///< 连接已建立
  H_DISCONNECTING,    ///< 正在断开连接
  H_DISCONNECTED      ///< 连接已断开
};

/**
 * @enum HttpMethod
 * @brief HTTP方法枚举
 */
enum HttpMethod { 
  METHOD_POST = 1,   ///< POST方法
  METHOD_GET,         ///< GET方法
  METHOD_HEAD         ///< HEAD方法
};

/**
 * @enum HttpVersion
 * @brief HTTP版本枚举
 */
enum HttpVersion { 
  HTTP_10 = 1,  ///< HTTP/1.0
  HTTP_11       ///< HTTP/1.1
};

/**
 * @class MimeType
 * @brief MIME类型管理类
 * 
 * @details 用于根据文件扩展名获取对应的MIME类型
 * 使用单例模式，线程安全地初始化MIME类型映射表
 */
class MimeType {
 private:
  static void init();  ///< 初始化MIME类型映射表（只执行一次）
  static std::unordered_map<std::string, std::string> mime;  ///< MIME类型映射表
  MimeType();  ///< 私有构造函数，防止实例化
  MimeType(const MimeType &m);  ///< 私有拷贝构造函数

 public:
  /**
   * @brief 根据文件扩展名获取MIME类型
   * 
   * @param suffix 文件扩展名（如".html"、".css"、".js"）
   * @return std::string 对应的MIME类型字符串（如"text/html"）
   * 
   * @details 如果找不到对应的MIME类型，返回默认值"text/html"
   */
  static std::string getMime(const std::string &suffix);

 private:
  static pthread_once_t once_control;  ///< 确保init()只执行一次的控制器
};

/**
 * @class HttpData
 * @brief HTTP请求处理类，负责单个HTTP连接的完整生命周期
 * 
 * @details 继承自enable_shared_from_this，支持智能指针管理
 * 使用状态机解析HTTP请求，支持GET、POST、HEAD方法
 * 支持静态文件服务和HTTP长连接（Keep-Alive）
 */
class HttpData : public std::enable_shared_from_this<HttpData> {
 public:
  /**
   * @brief 构造函数，初始化HTTP连接处理对象
   * 
   * @param loop 所属的事件循环
   * @param connfd 客户端连接的文件描述符
   */
  HttpData(EventLoop *loop, int connfd);
  
  /**
   * @brief 析构函数，关闭连接文件描述符
   */
  ~HttpData() { close(fd_); }
  
  /**
   * @brief 重置HTTP请求处理状态，用于处理下一个请求（Keep-Alive）
   * 
   * @details 清空文件名、路径、HTTP头等信息，重置状态机
   */
  void reset();
  
  /**
   * @brief 分离定时器，取消该连接的定时器关联
   */
  void seperateTimer();
  
  /**
   * @brief 关联定时器节点
   * 
   * @param mtimer 定时器节点的智能指针
   */
  void linkTimer(std::shared_ptr<TimerNode> mtimer) {
    // shared_ptr重载了bool, 但weak_ptr没有
    timer_ = mtimer;
  }
  
  /**
   * @brief 获取事件通道
   * 
   * @return std::shared_ptr<Channel> 事件通道的智能指针
   */
  std::shared_ptr<Channel> getChannel() { return channel_; }
  
  /**
   * @brief 获取所属的事件循环
   * 
   * @return EventLoop* 事件循环指针
   */
  EventLoop *getLoop() { return loop_; }
  
  /**
   * @brief 处理连接关闭
   * 
   * @details 从事件循环中移除通道，关闭连接
   */
  void handleClose();
  
  /**
   * @brief 新事件初始化
   * 
   * @details 将通道注册到事件循环的epoll中，开始监听IO事件
   */
  void newEvent();

 private:
  EventLoop *loop_;              ///< 所属的事件循环
  std::shared_ptr<Channel> channel_;  ///< 事件通道，封装文件描述符的IO事件
  int fd_;                       ///< 客户端连接的文件描述符
  std::string inBuffer_;         ///< 输入缓冲区，存储接收到的HTTP请求数据
  std::string outBuffer_;        ///< 输出缓冲区，存储要发送的HTTP响应数据
  bool error_;                   ///< 错误标志，表示处理过程中是否发生错误
  ConnectionState connectionState_;  ///< 连接状态（已连接/正在断开/已断开）

  HttpMethod method_;            ///< HTTP请求方法（GET/POST/HEAD）
  HttpVersion HTTPVersion_;      ///< HTTP版本（1.0/1.1）
  std::string fileName_;         ///< 请求的文件名（从URI中解析）
  std::string path_;             ///< 请求的路径
  int nowReadPos_;               ///< 当前读取位置（用于解析）
  ProcessState state_;           ///< 当前处理状态（状态机）
  ParseState hState_;           ///< HTTP头解析状态（状态机）
  bool keepAlive_;               ///< 是否保持连接（Keep-Alive）
  std::map<std::string, std::string> headers_;  ///< HTTP请求头映射表
  std::weak_ptr<TimerNode> timer_;  ///< 关联的定时器节点（弱引用）

  /**
   * @brief 处理读事件
   * 
   * @details 读取HTTP请求数据，解析请求，生成响应
   */
  void handleRead();
  
  /**
   * @brief 处理写事件
   * 
   * @details 发送HTTP响应数据到客户端
   */
  void handleWrite();
  
  /**
   * @brief 处理连接事件
   * 
   * @details 更新连接状态，管理定时器，更新epoll事件
   */
  void handleConn();
  
  /**
   * @brief 处理错误并发送错误响应
   * 
   * @param fd 文件描述符
   * @param err_num HTTP错误码（如404、400）
   * @param short_msg 错误消息
   */
  void handleError(int fd, int err_num, std::string short_msg);
  
  /**
   * @brief 解析HTTP请求URI
   * 
   * @return URIState 解析结果状态
   * 
   * @details 解析请求行，提取HTTP方法、URI、版本号
   */
  URIState parseURI();
  
  /**
   * @brief 解析HTTP请求头
   * 
   * @return HeaderState 解析结果状态
   * 
   * @details 使用状态机逐行解析HTTP头部
   */
  HeaderState parseHeaders();
  
  /**
   * @brief 分析请求并生成响应
   * 
   * @return AnalysisState 分析结果状态
   * 
   * @details 根据请求方法和URI，读取文件或处理请求，生成HTTP响应
   */
  AnalysisState analysisRequest();
};