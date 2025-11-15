/// @file Util.h
/// @brief 工具函数头文件，提供IO操作和socket配置函数
/// @author Lin Ya
/// @email xxbbb@vip.qq.com
/// 
/// @details 本文件提供了网络编程中常用的工具函数：
/// - 非阻塞IO读写函数
/// - Socket配置函数（非阻塞、Nagle算法、Linger选项等）
/// - 信号处理函数
/// - Socket创建和绑定函数

#pragma once
#include <cstdlib>  ///< 标准库函数
#include <string>   ///< 字符串类

/**
 * @brief 从文件描述符读取指定字节数的数据（非阻塞）
 * 
 * @param fd 文件描述符
 * @param buff 缓冲区指针
 * @param n 要读取的字节数
 * @return ssize_t 实际读取的字节数，-1表示错误，0表示对端关闭
 * 
 * @details 循环读取直到读取完n字节或遇到EAGAIN（非阻塞）
 */
ssize_t readn(int fd, void *buff, size_t n);

/**
 * @brief 从文件描述符读取数据到字符串（非阻塞，带zero标志）
 * 
 * @param fd 文件描述符
 * @param inBuffer 输入字符串缓冲区
 * @param zero 输出参数，如果对端关闭则为true
 * @return ssize_t 实际读取的字节数，-1表示错误
 * 
 * @details 循环读取直到遇到EAGAIN或对端关闭（read返回0）
 */
ssize_t readn(int fd, std::string &inBuffer, bool &zero);

/**
 * @brief 从文件描述符读取数据到字符串（非阻塞）
 * 
 * @param fd 文件描述符
 * @param inBuffer 输入字符串缓冲区
 * @return ssize_t 实际读取的字节数，-1表示错误
 * 
 * @details 循环读取直到遇到EAGAIN
 */
ssize_t readn(int fd, std::string &inBuffer);

/**
 * @brief 向文件描述符写入指定字节数的数据（非阻塞）
 * 
 * @param fd 文件描述符
 * @param buff 缓冲区指针
 * @param n 要写入的字节数
 * @return ssize_t 实际写入的字节数，-1表示错误
 * 
 * @details 循环写入直到写入完n字节或遇到EAGAIN
 */
ssize_t writen(int fd, void *buff, size_t n);

/**
 * @brief 向文件描述符写入字符串数据（非阻塞）
 * 
 * @param fd 文件描述符
 * @param sbuff 要写入的字符串
 * @return ssize_t 实际写入的字节数，-1表示错误
 * 
 * @details 循环写入直到写入完所有数据或遇到EAGAIN
 * 如果全部写入成功，清空字符串；否则保留未写入的部分
 */
ssize_t writen(int fd, std::string &sbuff);

/**
 * @brief 处理SIGPIPE信号，避免服务器因客户端断开连接而崩溃
 * 
 * @details 将SIGPIPE信号的处理方式设置为忽略（SIG_IGN）
 * 当向已关闭的socket写入数据时，不会触发SIGPIPE信号导致程序退出
 */
void handle_for_sigpipe();

/**
 * @brief 设置socket为非阻塞模式
 * 
 * @param fd 文件描述符
 * @return int 成功返回0，失败返回-1
 * 
 * @details 使用fcntl设置O_NONBLOCK标志
 */
int setSocketNonBlocking(int fd);

/**
 * @brief 禁用Nagle算法
 * 
 * @param fd 文件描述符
 * 
 * @details 设置TCP_NODELAY选项，减少小数据包的延迟
 * 适合需要即时交互的场景
 */
void setSocketNodelay(int fd);

/**
 * @brief 设置socket的Linger选项
 * 
 * @param fd 文件描述符
 * 
 * @details 设置SO_LINGER选项，关闭时等待30秒
 * 用于优雅关闭连接
 */
void setSocketNoLinger(int fd);

/**
 * @brief 关闭socket的写端（优雅关闭）
 * 
 * @param fd 文件描述符
 * 
 * @details 使用shutdown关闭写端，允许继续读取数据
 * 用于实现优雅关闭
 */
void shutDownWR(int fd);

/**
 * @brief 创建、绑定、监听socket
 * 
 * @param port 监听端口号
 * @return int 成功返回监听socket的文件描述符，失败返回-1
 * 
 * @details 执行流程：
 * 1. 创建IPv4 TCP socket
 * 2. 设置SO_REUSEADDR选项（允许地址重用）
 * 3. 绑定到指定端口（INADDR_ANY）
 * 4. 开始监听，最大等待队列长度为2048
 */
int socket_bind_listen(int port);