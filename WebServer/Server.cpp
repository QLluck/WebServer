// @Author Lin Ya
// @Email xxbbb@vip.qq.com
#include "Server.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <functional>
#include "Util.h"
#include "base/Logging.h"

/// @brief Server类的构造函数，初始化服务器实例
/// @param loop 主事件循环，负责处理监听socket的事件
/// @param threadNum 事件循环线程池中的线程数量
/// @param port 服务器监听的端口号
/// @returns 无
Server::Server(EventLoop *loop, int threadNum, int port)
    : loop_(loop), ///< 初始化主事件循环指针
      threadNum_(threadNum),  ///<初始化线程池数量
       // 创建事件循环线程池，传入主循环和线程数量
      eventLoopThreadPool_(new EventLoopThreadPool(loop_, threadNum)),
      started_(false),  ///< 初始化服务器启动状态为未启动
      acceptChannel_(new Channel(loop_)), ///< 创建处理accept事件的通道，关联主循环
      port_(port),///< 初始化监听端口

      listenFd_(socket_bind_listen(port_)) {

  acceptChannel_->setFd(listenFd_); ///< 为accept通道设置监听文件描述符
  
  handle_for_sigpipe();///< 处理SIGPIPE信号，避免服务器因客户端断开连接而崩溃
// 设置监听socket为非阻塞模式
  if (setSocketNonBlocking(listenFd_) < 0) {
    perror("set socket non block failed");///< 输出设置非阻塞失败的错误信息
    abort();///< 退出
  }
}

/// @brief 启动服务器，初始化事件循环线程池并配置监听通道，使服务器进入运行状态
/// @note 该函数会完成服务器启动的核心流程：启动线程池、配置监听事件、绑定回调函数、将通道加入事件循环
void Server::start() {
  // 启动事件循环线程池，创建并初始化所有子事件循环线程
  // 线程池启动后可处理后续接收的客户端连接
  eventLoopThreadPool_->start();

  // 配置accept通道的监听事件：
  // - EPOLLIN：表示监听读事件（当有新客户端连接请求时触发）
  // - EPOLLET：启用边缘触发模式（仅在事件状态变化时触发，提高高并发场景下的效率）
  // 注：注释掉的EPOLLONESHOT表示"一次性事件"（触发后需重新注册），当前未启用
  // acceptChannel_->setEvents(EPOLLIN | EPOLLET | EPOLLONESHOT);
  acceptChannel_->setEvents(EPOLLIN | EPOLLET);

  // 为accept通道绑定读事件回调函数：
  // 当监听到新连接请求（EPOLLIN事件）时，自动调用Server::handNewConn处理
  // std::bind将成员函数与当前Server对象（this）绑定，生成可调用的函数对象
  acceptChannel_->setReadHandler(bind(&Server::handNewConn, this));

  // 为accept通道绑定连接管理回调函数：
  // 通常用于处理通道与事件循环的关联逻辑（如更新poller中的通道状态）
  acceptChannel_->setConnHandler(bind(&Server::handThisConn, this));

  // 将accept通道添加到主事件循环的poller中，开始监控其事件
  // 第二个参数0表示不设置超时时间（持续监控，直到通道被移除）
  loop_->addToPoller(acceptChannel_, 0);

  // 更新服务器启动状态为已启动
  started_ = true;
}
/// @brief 处理新客户端连接的核心函数，由acceptChannel_的读事件触发
/// @note 当监听到新连接请求（EPOLLIN事件）时被调用，负责接收连接、初始化处理对象并分配到事件循环
void Server::handNewConn() {
    // 定义用于存储客户端地址信息的结构体（IPv4）
  struct sockaddr_in client_addr;
  // 将 client_addr 结构体的所有字节初始化为 0
  // 作用：清除结构体中的垃圾值，确保内存状态可控
  // 参数说明：
  //   &client_addr：要初始化的结构体地址
  //   0：初始化值（将内存填充为 0）
  //   sizeof(struct sockaddr_in)：要初始化的字节数（结构体总大小）

  memset(&client_addr, 0, sizeof(struct sockaddr_in));
  // 存储客户端地址结构体的大小，用于accept()系统调用
  socklen_t client_addr_len = sizeof(client_addr);
  int accept_fd = 0;// 新连接的文件描述符
  // 循环接收所有待处理的新连接（边缘触发模式下需一次性处理完）
  // accept()成功返回新连接的文件描述符，失败返回-1
  while ((accept_fd = accept(listenFd_, (struct sockaddr *)&client_addr, &client_addr_len)) > 0) {
    // 从事件循环线程池中获取下一个可用的事件循环（轮询策略）
    // 实现连接的负载均衡，将新连接分配到不同的子线程处理

    EventLoop *loop = eventLoopThreadPool_->getNextLoop();
    // 输出新连接的客户端IP和端口（需转换为本地字节序）
    LOG << "New connection from " << inet_ntoa(client_addr.sin_addr) << ":"
        << ntohs(client_addr.sin_port);
    // cout << "new connection" << endl;
    // cout << inet_ntoa(client_addr.sin_addr) << endl;
    // cout << ntohs(client_addr.sin_port) << endl;
    /*
    // TCP的保活机制默认是关闭的
    int optval = 0;
    socklen_t len_optval = 4;
    getsockopt(accept_fd, SOL_SOCKET,  SO_KEEPALIVE, &optval, &len_optval);
    cout << "optval ==" << optval << endl;
    */
    // 限制服务器的最大并发连接数
    if (accept_fd >= MAXFDS) {
      close(accept_fd);
      continue;
    }
    // 设为非阻塞模式
    if (setSocketNonBlocking(accept_fd) < 0) {
      LOG << "Set non block failed!";
      // perror("Set non block failed!");
      return;
    }
    // 禁用Nagle算法，减少TCP传输延  void handThisConn() { loop_->updatePoller(acceptChannel_); }迟（适合小数据即时交互）
    setSocketNodelay(accept_fd);
    // setSocketNoLinger(accept_fd);
    // 创建HttpData智能指针，封装新连接的HTTP协议处理逻辑
    // 关联对应的事件循环和文件描述符
    shared_ptr<HttpData> req_info(new HttpData(loop, accept_fd));
    // 将HttpData实例绑定到其内部的Channel上
    // 确保Channel触发事件时能安全访问对应的HttpData（避免悬空指针）
    req_info->getChannel()->setHolder(req_info);
    // 将新连接的初始化事件加入事件循环的任务队列
    // 确保在loop对应的线程中执行HttpData::newEvent（线程安全）
    loop->queueInLoop(std::bind(&HttpData::newEvent, req_info));
  }
  // 重置acceptChannel_的事件监听（边缘触发模式下需重新设置）
  // 确保下一次有新连接时能继续触发EPOLLIN事件
  acceptChannel_->setEvents(EPOLLIN | EPOLLET);
}