/// @file HttpData.cpp
/// @brief HTTP请求处理类实现文件
/// @author Lin Ya
/// @email xxbbb@vip.qq.com
/// 
/// @details 实现了HTTP请求的完整处理流程：
/// - HTTP请求解析（URI、Headers、Body）
/// - HTTP响应生成
/// - 静态文件服务
/// - Keep-Alive连接管理
/// - 错误处理

#include "HttpData.h"
#include <fcntl.h>      ///< 文件控制（open等）
#include <sys/mman.h>   ///< 内存映射（mmap）
#include <sys/stat.h>    ///< 文件状态（stat）
#include <iostream>     ///< 输入输出流
#include "Channel.h"    ///< 事件通道类
#include "EventLoop.h"  ///< 事件循环类
#include "Util.h"       ///< 工具函数
#include "time.h"       ///< 时间相关

using namespace std;

/// @brief MIME类型映射表的初始化控制器（确保只初始化一次）
pthread_once_t MimeType::once_control = PTHREAD_ONCE_INIT;

/// @brief MIME类型映射表（文件扩展名 -> MIME类型）
std::unordered_map<std::string, std::string> MimeType::mime;

const __uint32_t DEFAULT_EVENT = EPOLLIN | EPOLLET | EPOLLONESHOT;  ///< 默认epoll事件类型
const int DEFAULT_EXPIRED_TIME = 2000;              ///< 默认超时时间（毫秒）
const int DEFAULT_KEEP_ALIVE_TIME = 5 * 60 * 1000;  ///< Keep-Alive连接超时时间（5分钟，毫秒）

/// @brief favicon.ico文件的二进制数据（PNG格式，555字节）
/// @details 当客户端请求favicon.ico时，直接返回此数据，无需读取文件
char favicon[555] = {
    '\x89', 'P',    'N',    'G',    '\xD',  '\xA',  '\x1A', '\xA',  '\x0',
    '\x0',  '\x0',  '\xD',  'I',    'H',    'D',    'R',    '\x0',  '\x0',
    '\x0',  '\x10', '\x0',  '\x0',  '\x0',  '\x10', '\x8',  '\x6',  '\x0',
    '\x0',  '\x0',  '\x1F', '\xF3', '\xFF', 'a',    '\x0',  '\x0',  '\x0',
    '\x19', 't',    'E',    'X',    't',    'S',    'o',    'f',    't',
    'w',    'a',    'r',    'e',    '\x0',  'A',    'd',    'o',    'b',
    'e',    '\x20', 'I',    'm',    'a',    'g',    'e',    'R',    'e',
    'a',    'd',    'y',    'q',    '\xC9', 'e',    '\x3C', '\x0',  '\x0',
    '\x1',  '\xCD', 'I',    'D',    'A',    'T',    'x',    '\xDA', '\x94',
    '\x93', '9',    'H',    '\x3',  'A',    '\x14', '\x86', '\xFF', '\x5D',
    'b',    '\xA7', '\x4',  'R',    '\xC4', 'm',    '\x22', '\x1E', '\xA0',
    'F',    '\x24', '\x8',  '\x16', '\x16', 'v',    '\xA',  '6',    '\xBA',
    'J',    '\x9A', '\x80', '\x8',  'A',    '\xB4', 'q',    '\x85', 'X',
    '\x89', 'G',    '\xB0', 'I',    '\xA9', 'Q',    '\x24', '\xCD', '\xA6',
    '\x8',  '\xA4', 'H',    'c',    '\x91', 'B',    '\xB',  '\xAF', 'V',
    '\xC1', 'F',    '\xB4', '\x15', '\xCF', '\x22', 'X',    '\x98', '\xB',
    'T',    'H',    '\x8A', 'd',    '\x93', '\x8D', '\xFB', 'F',    'g',
    '\xC9', '\x1A', '\x14', '\x7D', '\xF0', 'f',    'v',    'f',    '\xDF',
    '\x7C', '\xEF', '\xE7', 'g',    'F',    '\xA8', '\xD5', 'j',    'H',
    '\x24', '\x12', '\x2A', '\x0',  '\x5',  '\xBF', 'G',    '\xD4', '\xEF',
    '\xF7', '\x2F', '6',    '\xEC', '\x12', '\x20', '\x1E', '\x8F', '\xD7',
    '\xAA', '\xD5', '\xEA', '\xAF', 'I',    '5',    'F',    '\xAA', 'T',
    '\x5F', '\x9F', '\x22', 'A',    '\x2A', '\x95', '\xA',  '\x83', '\xE5',
    'r',    '9',    'd',    '\xB3', 'Y',    '\x96', '\x99', 'L',    '\x6',
    '\xE9', 't',    '\x9A', '\x25', '\x85', '\x2C', '\xCB', 'T',    '\xA7',
    '\xC4', 'b',    '1',    '\xB5', '\x5E', '\x0',  '\x3',  'h',    '\x9A',
    '\xC6', '\x16', '\x82', '\x20', 'X',    'R',    '\x14', 'E',    '6',
    'S',    '\x94', '\xCB', 'e',    'x',    '\xBD', '\x5E', '\xAA', 'U',
    'T',    '\x23', 'L',    '\xC0', '\xE0', '\xE2', '\xC1', '\x8F', '\x0',
    '\x9E', '\xBC', '\x9',  'A',    '\x7C', '\x3E', '\x1F', '\x83', 'D',
    '\x22', '\x11', '\xD5', 'T',    '\x40', '\x3F', '8',    '\x80', 'w',
    '\xE5', '3',    '\x7',  '\xB8', '\x5C', '\x2E', 'H',    '\x92', '\x4',
    '\x87', '\xC3', '\x81', '\x40', '\x20', '\x40', 'g',    '\x98', '\xE9',
    '6',    '\x1A', '\xA6', 'g',    '\x15', '\x4',  '\xE3', '\xD7', '\xC8',
    '\xBD', '\x15', '\xE1', 'i',    '\xB7', 'C',    '\xAB', '\xEA', 'x',
    '\x2F', 'j',    'X',    '\x92', '\xBB', '\x18', '\x20', '\x9F', '\xCF',
    '3',    '\xC3', '\xB8', '\xE9', 'N',    '\xA7', '\xD3', 'l',    'J',
    '\x0',  'i',    '6',    '\x7C', '\x8E', '\xE1', '\xFE', 'V',    '\x84',
    '\xE7', '\x3C', '\x9F', 'r',    '\x2B', '\x3A', 'B',    '\x7B', '7',
    'f',    'w',    '\xAE', '\x8E', '\xE',  '\xF3', '\xBD', 'R',    '\xA9',
    'd',    '\x2',  'B',    '\xAF', '\x85', '2',    'f',    'F',    '\xBA',
    '\xC',  '\xD9', '\x9F', '\x1D', '\x9A', 'l',    '\x22', '\xE6', '\xC7',
    '\x3A', '\x2C', '\x80', '\xEF', '\xC1', '\x15', '\x90', '\x7',  '\x93',
    '\xA2', '\x28', '\xA0', 'S',    'j',    '\xB1', '\xB8', '\xDF', '\x29',
    '5',    'C',    '\xE',  '\x3F', 'X',    '\xFC', '\x98', '\xDA', 'y',
    'j',    'P',    '\x40', '\x0',  '\x87', '\xAE', '\x1B', '\x17', 'B',
    '\xB4', '\x3A', '\x3F', '\xBE', 'y',    '\xC7', '\xA',  '\x26', '\xB6',
    '\xEE', '\xD9', '\x9A', '\x60', '\x14', '\x93', '\xDB', '\x8F', '\xD',
    '\xA',  '\x2E', '\xE9', '\x23', '\x95', '\x29', 'X',    '\x0',  '\x27',
    '\xEB', 'n',    'V',    'p',    '\xBC', '\xD6', '\xCB', '\xD6', 'G',
    '\xAB', '\x3D', 'l',    '\x7D', '\xB8', '\xD2', '\xDD', '\xA0', '\x60',
    '\x83', '\xBA', '\xEF', '\x5F', '\xA4', '\xEA', '\xCC', '\x2',  'N',
    '\xAE', '\x5E', 'p',    '\x1A', '\xEC', '\xB3', '\x40', '9',    '\xAC',
    '\xFE', '\xF2', '\x91', '\x89', 'g',    '\x91', '\x85', '\x21', '\xA8',
    '\x87', '\xB7', 'X',    '\x7E', '\x7E', '\x85', '\xBB', '\xCD', 'N',
    'N',    'b',    't',    '\x40', '\xFA', '\x93', '\x89', '\xEC', '\x1E',
    '\xEC', '\x86', '\x2',  'H',    '\x26', '\x93', '\xD0', 'u',    '\x1D',
    '\x7F', '\x9',  '2',    '\x95', '\xBF', '\x1F', '\xDB', '\xD7', 'c',
    '\x8A', '\x1A', '\xF7', '\x5C', '\xC1', '\xFF', '\x22', 'J',    '\xC3',
    '\x87', '\x0',  '\x3',  '\x0',  'K',    '\xBB', '\xF8', '\xD6', '\x2A',
    'v',    '\x98', 'I',    '\x0',  '\x0',  '\x0',  '\x0',  'I',    'E',
    'N',    'D',    '\xAE', 'B',    '\x60', '\x82',
};

/**
 * @brief 初始化MIME类型映射表
 * 
 * @details 建立文件扩展名到MIME类型的映射关系
 * 支持常见的文件类型：HTML、CSS、JS、图片、视频、音频等
 * 
 * @note 此函数只执行一次（通过pthread_once保证）
 */
void MimeType::init() {
  mime[".html"] = "text/html";
  mime[".avi"] = "video/x-msvideo";
  mime[".bmp"] = "image/bmp";
  mime[".c"] = "text/plain";
  mime[".doc"] = "application/msword";
  mime[".gif"] = "image/gif";
  mime[".gz"] = "application/x-gzip";
  mime[".htm"] = "text/html";
  mime[".ico"] = "image/x-icon";
  mime[".jpg"] = "image/jpeg";
  mime[".css"] = "text/css";
  mime[".js"] = "application/javascript";
  mime[".png"] = "image/png";
  mime[".txt"] = "text/plain";
  mime[".mp3"] = "audio/mp3";
  mime["default"] = "text/html";  ///< 默认MIME类型
}

/**
 * @brief 根据文件扩展名获取MIME类型
 * 
 * @param suffix 文件扩展名（如".html"、".css"、".js"）
 * @return std::string 对应的MIME类型字符串
 * 
 * @details 如果找不到对应的MIME类型，返回默认值"text/html"
 * 使用pthread_once确保映射表只初始化一次
 */
std::string MimeType::getMime(const std::string &suffix) {
  pthread_once(&once_control, MimeType::init);  // 确保只初始化一次
  if (mime.find(suffix) == mime.end())
    return mime["default"];
  else
    return mime[suffix];
}

/**
 * @brief HttpData构造函数，初始化HTTP连接处理对象
 * 
 * @param loop 所属的事件循环
 * @param connfd 客户端连接的文件描述符
 * 
 * @details 初始化流程：
 * 1. 创建Channel对象关联连接的文件描述符
 * 2. 初始化所有成员变量（状态机、缓冲区等）
 * 3. 绑定事件处理回调函数（读、写、连接）
 */
HttpData::HttpData(EventLoop *loop, int connfd)
    : loop_(loop),
      channel_(new Channel(loop, connfd)),
      fd_(connfd),
      error_(false),
      connectionState_(H_CONNECTED),
      method_(METHOD_GET),
      HTTPVersion_(HTTP_11),
      nowReadPos_(0),
      state_(STATE_PARSE_URI),
      hState_(H_START),
      keepAlive_(false) {
  // loop_->queueInLoop(bind(&HttpData::setHandlers, this));
  channel_->setReadHandler(bind(&HttpData::handleRead, this));   // 绑定读事件处理
  channel_->setWriteHandler(bind(&HttpData::handleWrite, this)); // 绑定写事件处理
  channel_->setConnHandler(bind(&HttpData::handleConn, this));   // 绑定连接事件处理
}

/**
 * @brief 重置HTTP请求处理状态，用于处理下一个请求（Keep-Alive）
 * 
 * @details 清空文件名、路径、HTTP头等信息，重置状态机到初始状态
 * 分离定时器，但保留连接状态和Keep-Alive标志
 * 
 * @note 用于HTTP长连接场景，同一个连接可以处理多个请求
 */
void HttpData::reset() {
  // inBuffer_.clear();  // 不清空输入缓冲区，可能还有数据
  fileName_.clear();
  path_.clear();
  nowReadPos_ = 0;
  state_ = STATE_PARSE_URI;  // 重置状态机到URI解析阶段
  hState_ = H_START;          // 重置HTTP头解析状态机
  headers_.clear();
  // keepAlive_ = false;  // 保留Keep-Alive标志
  if (timer_.lock()) {
    shared_ptr<TimerNode> my_timer(timer_.lock());
    my_timer->clearReq();
    timer_.reset();
  }
}

/**
 * @brief 分离定时器，取消该连接的定时器关联
 * 
 * @details 清除定时器与HttpData的关联，标记定时器为deleted
 * 用于延迟删除策略，避免立即从优先队列中删除
 */
void HttpData::seperateTimer() {
  // cout << "seperateTimer" << endl;
  if (timer_.lock()) {
    shared_ptr<TimerNode> my_timer(timer_.lock());
    my_timer->clearReq();
    timer_.reset();
  }
}

/**
 * @brief 处理读事件，读取HTTP请求并解析
 * 
 * @details 这是HTTP请求处理的核心函数，使用状态机解析请求：
 * 
 * 处理流程：
 * 1. 读取数据到inBuffer_（非阻塞，可能只读取部分数据）
 * 2. 根据当前状态（state_）执行相应的解析步骤：
 *    - STATE_PARSE_URI：解析请求行（方法、URI、版本）
 *    - STATE_PARSE_HEADERS：解析HTTP头
 *    - STATE_RECV_BODY：接收POST请求体
 *    - STATE_ANALYSIS：分析请求并生成响应
 * 3. 如果解析完成，调用handleWrite()发送响应
 * 4. 如果处理完成且Keep-Alive，重置状态继续处理下一个请求
 * 
 * @note 使用do-while(false)结构，便于使用break跳出
 * 边缘触发模式下，需要一次性处理完所有数据
 */
void HttpData::handleRead() {
  __uint32_t &events_ = channel_->getEvents();
  do {
    bool zero = false;  ///< 对端是否关闭连接
    int read_num = readn(fd_, inBuffer_, zero);  // 读取数据（非阻塞）
    LOG << "Request: " << inBuffer_;
    if (connectionState_ == H_DISCONNECTING) {  // 连接正在断开
      inBuffer_.clear();
      break;
    }
    // cout << inBuffer_ << endl;
    if (read_num < 0) {  // 读取错误
      perror("1");
      error_ = true;
      handleError(fd_, 400, "Bad Request");
      break;
    }
    // else if (read_num == 0)
    // {
    //     error_ = true;
    //     break;
    // }
    else if (zero) {  // 对端关闭连接
      // 有请求出现但是读不到数据，可能是Request
      // Aborted，或者来自网络的数据没有达到等原因
      // 最可能是对端已经关闭了，统一按照对端已经关闭处理
      // error_ = true;
      connectionState_ = H_DISCONNECTING;
      if (read_num == 0) {
        // error_ = true;
        break;
      }
      // cout << "readnum == 0" << endl;
    }

    // 状态机：解析URI（请求行）
    if (state_ == STATE_PARSE_URI) {
      URIState flag = this->parseURI();
      if (flag == PARSE_URI_AGAIN)  // 需要更多数据
        break;
      else if (flag == PARSE_URI_ERROR) {  // 解析错误
        perror("2");
        LOG << "FD = " << fd_ << "," << inBuffer_ << "******";
        inBuffer_.clear();
        error_ = true;
        handleError(fd_, 400, "Bad Request");
        break;
      } else
        state_ = STATE_PARSE_HEADERS;  // 解析成功，进入下一阶段
    }
    
    // 状态机：解析HTTP头
    if (state_ == STATE_PARSE_HEADERS) {
      HeaderState flag = this->parseHeaders();
      if (flag == PARSE_HEADER_AGAIN)  // 需要更多数据
        break;
      else if (flag == PARSE_HEADER_ERROR) {  // 解析错误
        perror("3");
        error_ = true;
        handleError(fd_, 400, "Bad Request");
        break;
      }
      if (method_ == METHOD_POST) {
        // POST方法需要接收请求体
        state_ = STATE_RECV_BODY;
      } else {
        // GET/HEAD方法直接进入分析阶段
        state_ = STATE_ANALYSIS;
      }
    }
    
    // 状态机：接收POST请求体
    if (state_ == STATE_RECV_BODY) {
      int content_length = -1;
      if (headers_.find("Content-length") != headers_.end()) {
        content_length = stoi(headers_["Content-length"]);
      } else {
        // cout << "(state_ == STATE_RECV_BODY)" << endl;
        error_ = true;
        handleError(fd_, 400, "Bad Request: Lack of argument (Content-length)");
        break;
      }
      // 检查是否已接收完所有请求体数据
      if (static_cast<int>(inBuffer_.size()) < content_length) break;
      state_ = STATE_ANALYSIS;  // 接收完成，进入分析阶段
    }
    
    // 状态机：分析请求并生成响应
    if (state_ == STATE_ANALYSIS) {
      AnalysisState flag = this->analysisRequest();
      if (flag == ANALYSIS_SUCCESS) {
        state_ = STATE_FINISH;  // 分析成功，处理完成
        break;
      } else {
        // cout << "state_ == STATE_ANALYSIS" << endl;
        error_ = true;
        break;
      }
    }
  } while (false);
  // cout << "state_=" << state_ << endl;
  
  // 处理完成后的操作
  if (!error_) {
    if (outBuffer_.size() > 0) {
      handleWrite();  // 发送响应
      // events_ |= EPOLLOUT;
    }
    // error_ may change
    if (!error_ && state_ == STATE_FINISH) {
      this->reset();  // 重置状态，准备处理下一个请求（Keep-Alive）
      if (inBuffer_.size() > 0) {
        // 如果还有数据，继续处理（支持HTTP管线化）
        if (connectionState_ != H_DISCONNECTING) handleRead();
      }

      // if ((keepAlive_ || inBuffer_.size() > 0) && connectionState_ ==
      // H_CONNECTED)
      // {
      //     this->reset();
      //     events_ |= EPOLLIN;
      // }
    } else if (!error_ && connectionState_ != H_DISCONNECTED)
      events_ |= EPOLLIN;  // 继续监听读事件
  }
}

/**
 * @brief 处理写事件，发送HTTP响应数据
 * 
 * @details 将outBuffer_中的数据发送到客户端。
 * 如果数据未完全发送（非阻塞模式下可能遇到EAGAIN），
 * 继续监听EPOLLOUT事件，等待下次可写时继续发送。
 * 
 * @note 边缘触发模式下，需要一次性发送完所有数据
 */
void HttpData::handleWrite() {
  if (!error_ && connectionState_ != H_DISCONNECTED) {
    __uint32_t &events_ = channel_->getEvents();
    if (writen(fd_, outBuffer_) < 0) {  // 写入数据（非阻塞）
      perror("writen");
      events_ = 0;
      error_ = true;
    }
    // 如果还有数据未发送完，继续监听写事件
    if (outBuffer_.size() > 0) events_ |= EPOLLOUT;
  }
}

/**
 * @brief 处理连接事件，更新epoll事件和定时器
 * 
 * @details 根据连接状态和Keep-Alive标志，更新epoll中的事件监听和超时时间：
 * 
 * 1. 如果连接正常且有事件：
 *    - 如果同时有读和写事件，优先处理写事件
 *    - 设置边缘触发模式
 *    - 根据Keep-Alive设置超时时间
 * 
 * 2. 如果Keep-Alive：
 *    - 继续监听读事件，等待下一个请求
 *    - 设置较长的超时时间（5分钟）
 * 
 * 3. 如果非Keep-Alive：
 *    - 继续监听读事件，但设置较短的超时时间
 * 
 * 4. 如果连接正在断开且有写事件：
 *    - 只监听写事件，等待数据发送完成
 * 
 * 5. 其他情况：
 *    - 关闭连接
 */
void HttpData::handleConn() {
  seperateTimer();  // 分离定时器
  __uint32_t &events_ = channel_->getEvents();
  if (!error_ && connectionState_ == H_CONNECTED) {
    if (events_ != 0) {
      int timeout = DEFAULT_EXPIRED_TIME;  // 默认超时时间（2秒）
      if (keepAlive_) timeout = DEFAULT_KEEP_ALIVE_TIME;  // Keep-Alive超时时间（5分钟）
      if ((events_ & EPOLLIN) && (events_ & EPOLLOUT)) {
        // 同时有读和写事件，优先处理写事件
        events_ = __uint32_t(0);
        events_ |= EPOLLOUT;
      }
      // events_ |= (EPOLLET | EPOLLONESHOT);
      events_ |= EPOLLET;  // 边缘触发模式
      loop_->updatePoller(channel_, timeout);

    } else if (keepAlive_) {
      // Keep-Alive连接，继续监听读事件，等待下一个请求
      events_ |= (EPOLLIN | EPOLLET);
      // events_ |= (EPOLLIN | EPOLLET | EPOLLONESHOT);
      int timeout = DEFAULT_KEEP_ALIVE_TIME;
      loop_->updatePoller(channel_, timeout);
    } else {
      // 非Keep-Alive连接，设置较短的超时时间
      // cout << "close normally" << endl;
      // loop_->shutdown(channel_);
      // loop_->runInLoop(bind(&HttpData::handleClose, shared_from_this()));
      events_ |= (EPOLLIN | EPOLLET);
      // events_ |= (EPOLLIN | EPOLLET | EPOLLONESHOT);
      int timeout = (DEFAULT_KEEP_ALIVE_TIME >> 1);  // 超时时间减半
      loop_->updatePoller(channel_, timeout);
    }
  } else if (!error_ && connectionState_ == H_DISCONNECTING &&
             (events_ & EPOLLOUT)) {
    // 连接正在断开，但还有数据要发送，只监听写事件
    events_ = (EPOLLOUT | EPOLLET);
  } else {
    // 其他情况，关闭连接
    // cout << "close with errors" << endl;
    loop_->runInLoop(bind(&HttpData::handleClose, shared_from_this()));
  }
}

/**
 * @brief 解析HTTP请求URI（请求行）
 * 
 * @return URIState 解析结果状态
 * 
 * @details 解析HTTP请求的第一行，格式：METHOD URI HTTP/VERSION
 * 例如：GET /index.html HTTP/1.1
 * 
 * 解析流程：
 * 1. 查找请求行结束符（\r）
 * 2. 提取HTTP方法（GET/POST/HEAD）
 * 3. 提取URI（文件路径），去除查询参数（?后的部分）
 * 4. 提取HTTP版本（1.0/1.1）
 * 
 * @note 如果URI为空或只有"/"，默认返回index.html
 * 如果请求行不完整，返回PARSE_URI_AGAIN，等待更多数据
 */
URIState HttpData::parseURI() {
  string &str = inBuffer_;
  string cop = str;
  // 读到完整的请求行再开始解析请求（请求行以\r\n结束）
  size_t pos = str.find('\r', nowReadPos_);
  if (pos < 0) {
    return PARSE_URI_AGAIN;  // 请求行不完整，需要更多数据
  }
  // 去掉请求行所占的空间，节省空间
  string request_line = str.substr(0, pos);  // 提取请求行
  if (str.size() > pos + 1)
    str = str.substr(pos + 1);  // 移除已解析的请求行
  else
    str.clear();
  
  // 解析HTTP方法（GET/POST/HEAD）
  int posGet = request_line.find("GET");
  int posPost = request_line.find("POST");
  int posHead = request_line.find("HEAD");

  if (posGet >= 0) {
    pos = posGet;
    method_ = METHOD_GET;
  } else if (posPost >= 0) {
    pos = posPost;
    method_ = METHOD_POST;
  } else if (posHead >= 0) {
    pos = posHead;
    method_ = METHOD_HEAD;
  } else {
    return PARSE_URI_ERROR;
  }

  // 解析文件名（URI）
  pos = request_line.find("/", pos);  // 查找URI开始位置
  if (pos < 0) {
    // URI为空，默认返回index.html
    fileName_ = "index.html";
    HTTPVersion_ = HTTP_11;
    return PARSE_URI_SUCCESS;
  } else {
    size_t _pos = request_line.find(' ', pos);  // 查找URI结束位置
    if (_pos < 0)
      return PARSE_URI_ERROR;
    else {
      if (_pos - pos > 1) {
        fileName_ = request_line.substr(pos + 1, _pos - pos - 1);  // 提取文件名
        size_t __pos = fileName_.find('?');  // 查找查询参数开始位置
        if (__pos >= 0) {
          // 去除查询参数（?后的部分）
          fileName_ = fileName_.substr(0, __pos);
        }
      }
      else
        fileName_ = "index.html";  // URI只有"/"，默认index.html
    }
    pos = _pos;
  }
  // cout << "fileName_: " << fileName_ << endl;
  
  // 解析HTTP版本号
  pos = request_line.find("/", pos);  // 查找版本号开始位置（HTTP/1.1中的"/"）
  if (pos < 0)
    return PARSE_URI_ERROR;
  else {
    if (request_line.size() - pos <= 3)
      return PARSE_URI_ERROR;
    else {
      string ver = request_line.substr(pos + 1, 3);  // 提取版本号（如"1.1"）
      if (ver == "1.0")
        HTTPVersion_ = HTTP_10;
      else if (ver == "1.1")
        HTTPVersion_ = HTTP_11;
      else
        return PARSE_URI_ERROR;  // 不支持的版本
    }
  }
  return PARSE_URI_SUCCESS;
}

/**
 * @brief 解析HTTP请求头
 * 
 * @return HeaderState 解析结果状态
 * 
 * @details 使用状态机逐字符解析HTTP头部，格式：Key: Value\r\n
 * 
 * 状态机流程：
 * H_START → H_KEY → H_COLON → H_SPACES_AFTER_COLON → 
 * H_VALUE → H_CR → H_LF → (下一个header或H_END_CR → H_END_LF)
 * 
 * 解析完每个header后，存储到headers_映射表中
 * 遇到空行（\r\n\r\n）表示头部解析完成
 * 
 * @note 如果头部不完整，返回PARSE_HEADER_AGAIN，等待更多数据
 */
HeaderState HttpData::parseHeaders() {
  string &str = inBuffer_;
  int key_start = -1, key_end = -1, value_start = -1, value_end = -1;  ///< header键值对的起止位置
  int now_read_line_begin = 0;  ///< 当前行的开始位置
  bool notFinish = true;        ///< 是否解析完成
  size_t i = 0;
  for (; i < str.size() && notFinish; ++i) {
    switch (hState_) {
      case H_START: {  ///< 开始状态，跳过空白字符
        if (str[i] == '\n' || str[i] == '\r') break;
        hState_ = H_KEY;
        key_start = i;
        now_read_line_begin = i;
        break;
      }
      case H_KEY: {  ///< 解析键（header name）
        if (str[i] == ':') {
          key_end = i;
          if (key_end - key_start <= 0) return PARSE_HEADER_ERROR;
          hState_ = H_COLON;
        } else if (str[i] == '\n' || str[i] == '\r')
          return PARSE_HEADER_ERROR;
        break;
      }
      case H_COLON: {  ///< 遇到冒号
        if (str[i] == ' ') {
          hState_ = H_SPACES_AFTER_COLON;
        } else
          return PARSE_HEADER_ERROR;
        break;
      }
      case H_SPACES_AFTER_COLON: {  ///< 冒号后的空格
        hState_ = H_VALUE;
        value_start = i;
        break;
      }
      case H_VALUE: {  ///< 解析值（header value）
        if (str[i] == '\r') {
          hState_ = H_CR;
          value_end = i;
          if (value_end - value_start <= 0) return PARSE_HEADER_ERROR;
        } else if (i - value_start > 255)  // 值太长
          return PARSE_HEADER_ERROR;
        break;
      }
      case H_CR: {  ///< 回车符
        if (str[i] == '\n') {
          hState_ = H_LF;
          // 提取键值对并存储
          string key(str.begin() + key_start, str.begin() + key_end);
          string value(str.begin() + value_start, str.begin() + value_end);
          headers_[key] = value;
          now_read_line_begin = i;
        } else
          return PARSE_HEADER_ERROR;
        break;
      }
      case H_LF: {  ///< 换行符
        if (str[i] == '\r') {
          hState_ = H_END_CR;  // 可能是头部结束（空行）
        } else {
          key_start = i;  // 下一个header开始
          hState_ = H_KEY;
        }
        break;
      }
      case H_END_CR: {  ///< 头部结束的回车符
        if (str[i] == '\n') {
          hState_ = H_END_LF;  // 头部解析完成
        } else
          return PARSE_HEADER_ERROR;
        break;
      }
      case H_END_LF: {  ///< 头部结束的换行符（空行）
        notFinish = false;
        key_start = i;
        now_read_line_begin = i;
        break;
      }
    }
  }
  if (hState_ == H_END_LF) {
    str = str.substr(i);  // 移除已解析的头部
    return PARSE_HEADER_SUCCESS;
  }
  str = str.substr(now_read_line_begin);  // 保留未解析的部分
  return PARSE_HEADER_AGAIN;  // 需要更多数据
}

/**
 * @brief 分析HTTP请求并生成响应
 * 
 * @return AnalysisState 分析结果状态
 * 
 * @details 根据HTTP方法和请求的文件，生成相应的HTTP响应：
 * 
 * 1. POST方法：
 *    - 当前未实现（有OpenCV图像拼接的注释代码）
 * 
 * 2. GET/HEAD方法：
 *    - 检查Keep-Alive头，设置keepAlive_标志
 *    - 根据文件扩展名确定MIME类型
 *    - 特殊处理：
 *      * "hello"：返回"Hello World"文本
 *      * "favicon.ico"：返回内置的favicon数据
 *    - 静态文件服务：
 *      * 使用mmap将文件映射到内存
 *      * 生成HTTP响应头（状态码、Content-Type、Content-Length等）
 *      * 将文件内容添加到响应体
 *    - 错误处理：
 *      * 文件不存在：返回404错误
 *      * 文件是目录：返回400错误
 * 
 * @note 使用mmap提高大文件传输性能
 */
AnalysisState HttpData::analysisRequest() {
  if (method_ == METHOD_POST) {
    // ------------------------------------------------------
    // My CV stitching handler which requires OpenCV library
    // ------------------------------------------------------
    // POST方法处理（当前未实现，有OpenCV图像拼接的示例代码）
    // string header;
    // header += string("HTTP/1.1 200 OK\r\n");
    // if(headers_.find("Connection") != headers_.end() &&
    // headers_["Connection"] == "Keep-Alive")
    // {
    //     keepAlive_ = true;
    //     header += string("Connection: Keep-Alive\r\n") + "Keep-Alive:
    //     timeout=" + to_string(DEFAULT_KEEP_ALIVE_TIME) + "\r\n";
    // }
    // int length = stoi(headers_["Content-length"]);
    // vector<char> data(inBuffer_.begin(), inBuffer_.begin() + length);
    // Mat src = imdecode(data, CV_LOAD_IMAGE_ANYDEPTH|CV_LOAD_IMAGE_ANYCOLOR);
    // //imwrite("receive.bmp", src);
    // Mat res = stitch(src);
    // vector<uchar> data_encode;
    // imencode(".png", res, data_encode);
    // header += string("Content-length: ") + to_string(data_encode.size()) +
    // "\r\n\r\n";
    // outBuffer_ += header + string(data_encode.begin(), data_encode.end());
    // inBuffer_ = inBuffer_.substr(length);
    // return ANALYSIS_SUCCESS;
  } else if (method_ == METHOD_GET || method_ == METHOD_HEAD) {
    string header;
    header += "HTTP/1.1 200 OK\r\n";
    if (headers_.find("Connection") != headers_.end() &&
        (headers_["Connection"] == "Keep-Alive" ||
         headers_["Connection"] == "keep-alive")) {
      keepAlive_ = true;
      header += string("Connection: Keep-Alive\r\n") + "Keep-Alive: timeout=" +
                to_string(DEFAULT_KEEP_ALIVE_TIME) + "\r\n";
    }
    int dot_pos = fileName_.find('.');
    string filetype;
    if (dot_pos < 0)
      filetype = MimeType::getMime("default");
    else
      filetype = MimeType::getMime(fileName_.substr(dot_pos));

    // echo test
    if (fileName_ == "hello") {
      outBuffer_ =
          "HTTP/1.1 200 OK\r\nContent-type: text/plain\r\n\r\nHello World";
      return ANALYSIS_SUCCESS;
    }
    if (fileName_ == "favicon.ico") {
      header += "Content-Type: image/png\r\n";
      header += "Content-Length: " + to_string(sizeof favicon) + "\r\n";
      header += "Server: LinYa's Web Server\r\n";

      header += "\r\n";
      outBuffer_ += header;
      outBuffer_ += string(favicon, favicon + sizeof favicon);
      ;
      return ANALYSIS_SUCCESS;
    }

    struct stat sbuf;
    if (stat(fileName_.c_str(), &sbuf) < 0) {
      header.clear();
      handleError(fd_, 404, "Not Found!");
      return ANALYSIS_ERROR;
    }
    header += "Content-Type: " + filetype + "\r\n";
    header += "Content-Length: " + to_string(sbuf.st_size) + "\r\n";
    header += "Server: LinYa's Web Server\r\n";
    // 头部结束
    header += "\r\n";
    outBuffer_ += header;

    if (method_ == METHOD_HEAD) return ANALYSIS_SUCCESS;

    int src_fd = open(fileName_.c_str(), O_RDONLY, 0);
    if (src_fd < 0) {
      outBuffer_.clear();
      handleError(fd_, 404, "Not Found!");
      return ANALYSIS_ERROR;
    }
    void *mmapRet = mmap(NULL, sbuf.st_size, PROT_READ, MAP_PRIVATE, src_fd, 0);
    close(src_fd);
    if (mmapRet == (void *)-1) {
      munmap(mmapRet, sbuf.st_size);
      outBuffer_.clear();
      handleError(fd_, 404, "Not Found!");
      return ANALYSIS_ERROR;
    }
    char *src_addr = static_cast<char *>(mmapRet);
    outBuffer_ += string(src_addr, src_addr + sbuf.st_size);
    ;
    munmap(mmapRet, sbuf.st_size);
    return ANALYSIS_SUCCESS;
  }
  return ANALYSIS_ERROR;
}

void HttpData::handleError(int fd, int err_num, string short_msg) {
  short_msg = " " + short_msg;
  char send_buff[4096];
  string body_buff, header_buff;
  body_buff += "<html><title>哎~出错了</title>";
  body_buff += "<body bgcolor=\"ffffff\">";
  body_buff += to_string(err_num) + short_msg;
  body_buff += "<hr><em> LinYa's Web Server</em>\n</body></html>";

  header_buff += "HTTP/1.1 " + to_string(err_num) + short_msg + "\r\n";
  header_buff += "Content-Type: text/html\r\n";
  header_buff += "Connection: Close\r\n";
  header_buff += "Content-Length: " + to_string(body_buff.size()) + "\r\n";
  header_buff += "Server: LinYa's Web Server\r\n";
  ;
  header_buff += "\r\n";
  // 错误处理不考虑writen不完的情况
  sprintf(send_buff, "%s", header_buff.c_str());
  writen(fd, send_buff, strlen(send_buff));
  sprintf(send_buff, "%s", body_buff.c_str());
  writen(fd, send_buff, strlen(send_buff));
}

void HttpData::handleClose() {
  connectionState_ = H_DISCONNECTED;
  shared_ptr<HttpData> guard(shared_from_this());
  loop_->removeFromPoller(channel_);
}

void HttpData::newEvent() {
  channel_->setEvents(DEFAULT_EVENT);
  loop_->addToPoller(channel_, DEFAULT_EXPIRED_TIME);
}
