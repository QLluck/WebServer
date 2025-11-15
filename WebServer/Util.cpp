/// @file Util.cpp
/// @brief 工具函数实现文件
/// @author Lin Ya
/// @email xxbbb@vip.qq.com
/// 
/// @details 实现了网络编程中常用的工具函数，包括IO操作和socket配置

#include "Util.h"

#include <errno.h>         ///< 错误码定义
#include <fcntl.h>         ///< 文件控制（fcntl）
#include <netinet/in.h>    ///< 网络地址结构
#include <netinet/tcp.h>   ///< TCP协议选项
#include <signal.h>        ///< 信号处理
#include <string.h>        ///< 字符串操作
#include <sys/socket.h>    ///< socket系统调用
#include <unistd.h>        ///< POSIX标准定义

const int MAX_BUFF = 4096;  ///< 读取缓冲区最大大小（字节）
/**
 * @brief 从文件描述符读取指定字节数的数据（非阻塞）
 * 
 * @param fd 文件描述符
 * @param buff 缓冲区指针
 * @param n 要读取的字节数
 * @return ssize_t 实际读取的字节数，-1表示错误，0表示对端关闭
 * 
 * @details 循环读取直到读取完n字节或遇到以下情况：
 * - EAGAIN：非阻塞模式下没有数据可读，返回已读取的字节数
 * - EINTR：被信号中断，继续读取
 * - read返回0：对端关闭连接，返回已读取的字节数
 * - 其他错误：返回-1
 */
ssize_t readn(int fd, void *buff, size_t n) {
  size_t nleft = n;        ///< 剩余要读取的字节数
  ssize_t nread = 0;       ///< 本次读取的字节数
  ssize_t readSum = 0;     ///< 累计读取的字节数
  char *ptr = (char *)buff;  ///< 缓冲区指针，用于移动位置
  while (nleft > 0) {
    if ((nread = read(fd, ptr, nleft)) < 0) {
      if (errno == EINTR)  // 被信号中断，继续读取
        nread = 0;
      else if (errno == EAGAIN) {  // 非阻塞模式下没有数据可读
        return readSum;
      } else {
        return -1;  // 其他错误
      }
    } else if (nread == 0)  // 对端关闭连接
      break;
    readSum += nread;
    nleft -= nread;
    ptr += nread;  // 移动缓冲区指针
  }
  return readSum;
}

/**
 * @brief 从文件描述符读取数据到字符串（非阻塞，带zero标志）
 * 
 * @param fd 文件描述符
 * @param inBuffer 输入字符串缓冲区（输出参数）
 * @param zero 输出参数，如果对端关闭则为true
 * @return ssize_t 实际读取的字节数，-1表示错误
 * 
 * @details 循环读取直到遇到以下情况：
 * - EAGAIN：非阻塞模式下没有数据可读，返回已读取的字节数
 * - read返回0：对端关闭连接，设置zero=true并返回
 * - 其他错误：返回-1
 * 
 * @note 每次最多读取MAX_BUFF字节，循环读取直到缓冲区满或遇到EAGAIN
 */
ssize_t readn(int fd, std::string &inBuffer, bool &zero) {
  ssize_t nread = 0;       ///< 本次读取的字节数
  ssize_t readSum = 0;     ///< 累计读取的字节数
  while (true) {
    char buff[MAX_BUFF];   ///< 临时缓冲区
    if ((nread = read(fd, buff, MAX_BUFF)) < 0) {
      if (errno == EINTR)  // 被信号中断，继续读取
        continue;
      else if (errno == EAGAIN) {  // 非阻塞模式下没有数据可读
        return readSum;
      } else {
        perror("read error");
        return -1;
      }
    } else if (nread == 0) {  // 对端关闭连接
      // printf("redsum = %d\n", readSum);
      zero = true;
      break;
    }
    // printf("before inBuffer.size() = %d\n", inBuffer.size());
    // printf("nread = %d\n", nread);
    readSum += nread;
    // buff += nread;
    inBuffer += std::string(buff, buff + nread);  // 追加到字符串
    // printf("after inBuffer.size() = %d\n", inBuffer.size());
  }
  return readSum;
}

/**
 * @brief 从文件描述符读取数据到字符串（非阻塞）
 * 
 * @param fd 文件描述符
 * @param inBuffer 输入字符串缓冲区（输出参数）
 * @return ssize_t 实际读取的字节数，-1表示错误
 * 
 * @details 与readn(fd, inBuffer, zero)类似，但不设置zero标志
 * 循环读取直到遇到EAGAIN或对端关闭
 */
ssize_t readn(int fd, std::string &inBuffer) {
  ssize_t nread = 0;       ///< 本次读取的字节数
  ssize_t readSum = 0;     ///< 累计读取的字节数
  while (true) {
    char buff[MAX_BUFF];   ///< 临时缓冲区
    if ((nread = read(fd, buff, MAX_BUFF)) < 0) {
      if (errno == EINTR)  // 被信号中断，继续读取
        continue;
      else if (errno == EAGAIN) {  // 非阻塞模式下没有数据可读
        return readSum;
      } else {
        perror("read error");
        return -1;
      }
    } else if (nread == 0) {  // 对端关闭连接
      // printf("redsum = %d\n", readSum);
      break;
    }
    // printf("before inBuffer.size() = %d\n", inBuffer.size());
    // printf("nread = %d\n", nread);
    readSum += nread;
    // buff += nread;
    inBuffer += std::string(buff, buff + nread);  // 追加到字符串
    // printf("after inBuffer.size() = %d\n", inBuffer.size());
  }
  return readSum;
}

/**
 * @brief 向文件描述符写入指定字节数的数据（非阻塞）
 * 
 * @param fd 文件描述符
 * @param buff 缓冲区指针
 * @param n 要写入的字节数
 * @return ssize_t 实际写入的字节数，-1表示错误
 * 
 * @details 循环写入直到写入完n字节或遇到以下情况：
 * - EAGAIN：非阻塞模式下缓冲区满，返回已写入的字节数
 * - EINTR：被信号中断，继续写入
 * - 其他错误：返回-1
 */
ssize_t writen(int fd, void *buff, size_t n) {
  size_t nleft = n;          ///< 剩余要写入的字节数
  ssize_t nwritten = 0;      ///< 本次写入的字节数
  ssize_t writeSum = 0;      ///< 累计写入的字节数
  char *ptr = (char *)buff;   ///< 缓冲区指针，用于移动位置
  while (nleft > 0) {
    if ((nwritten = write(fd, ptr, nleft)) <= 0) {
      if (nwritten < 0) {
        if (errno == EINTR) {  // 被信号中断，继续写入
          nwritten = 0;
          continue;
        } else if (errno == EAGAIN) {  // 非阻塞模式下缓冲区满
          return writeSum;
        } else
          return -1;  // 其他错误
      }
    }
    writeSum += nwritten;
    nleft -= nwritten;
    ptr += nwritten;  // 移动缓冲区指针
  }
  return writeSum;
}

/**
 * @brief 向文件描述符写入字符串数据（非阻塞）
 * 
 * @param fd 文件描述符
 * @param sbuff 要写入的字符串（输入输出参数）
 * @return ssize_t 实际写入的字节数，-1表示错误
 * 
 * @details 循环写入直到写入完所有数据或遇到EAGAIN。
 * 如果全部写入成功，清空字符串；否则保留未写入的部分。
 * 
 * @note 字符串会被修改：如果未完全写入，保留剩余部分
 */
ssize_t writen(int fd, std::string &sbuff) {
  size_t nleft = sbuff.size();  ///< 剩余要写入的字节数
  ssize_t nwritten = 0;          ///< 本次写入的字节数
  ssize_t writeSum = 0;          ///< 累计写入的字节数
  const char *ptr = sbuff.c_str();  ///< 字符串数据指针
  while (nleft > 0) {
    if ((nwritten = write(fd, ptr, nleft)) <= 0) {
      if (nwritten < 0) {
        if (errno == EINTR) {  // 被信号中断，继续写入
          nwritten = 0;
          continue;
        } else if (errno == EAGAIN)  // 非阻塞模式下缓冲区满
          break;
        else
          return -1;  // 其他错误
      }
    }
    writeSum += nwritten;
    nleft -= nwritten;
    ptr += nwritten;  // 移动指针
  }
  // 如果全部写入成功，清空字符串；否则保留未写入的部分
  if (writeSum == static_cast<int>(sbuff.size()))
    sbuff.clear();
  else
    sbuff = sbuff.substr(writeSum);
  return writeSum;
}

/**
 * @brief 处理SIGPIPE信号，避免服务器因客户端断开连接而崩溃
 * 
 * @details 将SIGPIPE信号的处理方式设置为忽略（SIG_IGN）。
 * 当向已关闭的socket写入数据时，不会触发SIGPIPE信号导致程序退出。
 * 
 * @note SIGPIPE默认行为是终止进程，这在服务器中是不期望的
 */
void handle_for_sigpipe() {
  struct sigaction sa;
  memset(&sa, '\0', sizeof(sa));
  sa.sa_handler = SIG_IGN;  // 忽略信号
  sa.sa_flags = 0;
  if (sigaction(SIGPIPE, &sa, NULL)) return;
}

/**
 * @brief 设置socket为非阻塞模式
 * 
 * @param fd 文件描述符
 * @return int 成功返回0，失败返回-1
 * 
 * @details 使用fcntl获取当前标志，添加O_NONBLOCK标志后设置回去
 * 非阻塞模式下，read/write不会阻塞，立即返回
 */
int setSocketNonBlocking(int fd) {
  int flag = fcntl(fd, F_GETFL, 0);  // 获取当前标志
  if (flag == -1) return -1;

  flag |= O_NONBLOCK;  // 添加非阻塞标志
  if (fcntl(fd, F_SETFL, flag) == -1) return -1;  // 设置新标志
  return 0;
}

/**
 * @brief 禁用Nagle算法
 * 
 * @param fd 文件描述符
 * 
 * @details 设置TCP_NODELAY选项，禁用Nagle算法。
 * Nagle算法会合并小数据包，增加延迟。
 * 禁用后，小数据包会立即发送，适合需要即时交互的场景（如HTTP请求）
 */
void setSocketNodelay(int fd) {
  int enable = 1;
  setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (void *)&enable, sizeof(enable));
}

/**
 * @brief 设置socket的Linger选项
 * 
 * @param fd 文件描述符
 * 
 * @details 设置SO_LINGER选项，关闭socket时等待30秒。
 * 用于优雅关闭连接，确保数据完全发送
 * 
 * @note 当前代码中未使用此函数
 */
void setSocketNoLinger(int fd) {
  struct linger linger_;
  linger_.l_onoff = 1;      // 启用Linger选项
  linger_.l_linger = 30;     // 等待30秒
  setsockopt(fd, SOL_SOCKET, SO_LINGER, (const char *)&linger_,
             sizeof(linger_));
}

/**
 * @brief 关闭socket的写端（优雅关闭）
 * 
 * @param fd 文件描述符
 * 
 * @details 使用shutdown关闭写端（SHUT_WR），允许继续读取数据。
 * 用于实现优雅关闭：先关闭写端，等待对端关闭，再关闭读端
 */
void shutDownWR(int fd) {
  shutdown(fd, SHUT_WR);
  // printf("shutdown\n");
}

/**
 * @brief 创建、绑定、监听socket
 * 
 * @param port 监听端口号
 * @return int 成功返回监听socket的文件描述符，失败返回-1
 * 
 * @details 执行流程：
 * 1. 检查端口号是否有效（0-65535）
 * 2. 创建IPv4 TCP socket（AF_INET + SOCK_STREAM）
 * 3. 设置SO_REUSEADDR选项（允许地址重用，避免"Address already in use"错误）
 * 4. 绑定到指定端口（INADDR_ANY表示监听所有网络接口）
 * 5. 开始监听，最大等待队列长度为2048
 * 
 * @note 返回的socket需要设置为非阻塞模式才能用于epoll
 */
int socket_bind_listen(int port) {
  // 检查port值，取正确区间范围
  if (port < 0 || port > 65535) return -1;

  // 创建socket(IPv4 + TCP)，返回监听描述符
  int listen_fd = 0;
  if ((listen_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) return -1;

  // 消除bind时"Address already in use"错误
  // SO_REUSEADDR允许重用处于TIME_WAIT状态的地址
  int optval = 1;
  if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &optval,
                 sizeof(optval)) == -1) {
    close(listen_fd);
    return -1;
  }

  // 设置服务器IP和Port，和监听描述符绑定
  struct sockaddr_in server_addr;
  bzero((char *)&server_addr, sizeof(server_addr));  // 清零
  server_addr.sin_family = AF_INET;                   // IPv4
  server_addr.sin_addr.s_addr = htonl(INADDR_ANY);    // 监听所有网络接口
  server_addr.sin_port = htons((unsigned short)port); // 端口号（网络字节序）
  if (bind(listen_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) ==
      -1) {
    close(listen_fd);
    return -1;
  }

  // 开始监听，最大等待队列长度为2048
  if (listen(listen_fd, 2048) == -1) {
    close(listen_fd);
    return -1;
  }

  // 无效监听描述符（冗余检查）
  if (listen_fd == -1) {
    close(listen_fd);
    return -1;
  }
  return listen_fd;
}