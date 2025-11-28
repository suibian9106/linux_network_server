#include "../include/server.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <iostream>
#include <cerrno>
#include <vector>

EpollServer::EpollServer(const ServerConfig& config) 
    : config_(config), listen_fd_(-1), epoll_fd_(-1), running_(false) {
}

EpollServer::~EpollServer() {
    stop();
}

bool EpollServer::initialize() {
    if (!setupListenSocket()) {
        std::cerr << "Failed to setup listen socket" << std::endl;
        return false;
    }
    
    if (!setupEpoll()) {
        std::cerr << "Failed to setup epoll" << std::endl;
        close(listen_fd_);
        return false;
    }
    
    std::cout << "Server initialized on port " << config_.port << std::endl;
    return true;
}

bool EpollServer::setupListenSocket() {
    // 创建监听socket
    listen_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd_ == -1) {
        std::cerr << "Create socket failed: " << strerror(errno) << std::endl;
        return false;
    }
    
    // 设置SO_REUSEADDR
    int opt = 1;
    if (setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        std::cerr << "Set SO_REUSEADDR failed: " << strerror(errno) << std::endl;
        close(listen_fd_);
        return false;
    }
    
    // 绑定地址
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(config_.port);
    
    if (bind(listen_fd_, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        std::cerr << "Bind failed: " << strerror(errno) << std::endl;
        close(listen_fd_);
        return false;
    }
    
    // 开始监听
    if (listen(listen_fd_, 128) < 0) {
        std::cerr << "Listen failed: " << strerror(errno) << std::endl;
        close(listen_fd_);
        return false;
    }
    
    // 设置为非阻塞模式
    int flags = fcntl(listen_fd_, F_GETFL, 0);
    if (flags == -1) {
        std::cerr << "Get socket flags failed: " << strerror(errno) << std::endl;
        close(listen_fd_);
        return false;
    }
    if (fcntl(listen_fd_, F_SETFL, flags | O_NONBLOCK) == -1) {
        std::cerr << "Set non-blocking failed: " << strerror(errno) << std::endl;
        close(listen_fd_);
        return false;
    }
    
    return true;
}

bool EpollServer::setupEpoll() {
    // 创建epoll实例
    epoll_fd_ = epoll_create1(0);
    if (epoll_fd_ == -1) {
        std::cerr << "Create epoll failed: " << strerror(errno) << std::endl;
        return false;
    }
    
    // 添加监听socket到epoll
    addEpollEvent(listen_fd_, EPOLLIN | (config_.use_et_mode ? EPOLLET : 0));
    
    return true;
}

void EpollServer::run() {
    if (listen_fd_ == -1 || epoll_fd_ == -1) {
        std::cerr << "Server not initialized" << std::endl;
        return;
    }
    
    running_ = true;
    struct epoll_event events[config_.max_events];
    
    std::cout << "Server started, waiting for connections..." << std::endl;
    
    while (running_) {
        int num_events = epoll_wait(epoll_fd_, events, config_.max_events, config_.timeout_ms);
        
        if (num_events == -1) {
            if (errno == EINTR) {
                continue; // 被信号中断，继续
            }
            std::cerr << "Epoll wait failed: " << strerror(errno) << std::endl;
            break;
        }
        
        if (num_events == 0) {
            // 超时，可以在这里处理超时逻辑
            continue;
        }
        
        for (int i = 0; i < num_events; ++i) {
            int fd = events[i].data.fd;
            uint32_t event_type = events[i].events;
            
            if (fd == listen_fd_) {
                // 新连接
                handleNewConnection();
            } else if (event_type & (EPOLLIN | EPOLLERR | EPOLLHUP)) {
                // 客户端数据可读或错误
                if (event_type & (EPOLLERR | EPOLLHUP)) {
                    handleClientClose(fd);
                } else {
                    handleClientData(fd);
                }
            }
        }
    }
}

void EpollServer::stop() {
    running_ = false;
    
    if (epoll_fd_ != -1) {
        close(epoll_fd_);
        epoll_fd_ = -1;
    }
    
    if (listen_fd_ != -1) {
        close(listen_fd_);
        listen_fd_ = -1;
    }
    
    // 关闭所有客户端连接
    for (auto& client : client_buffers_) {
        close(client);
    }
    client_buffers_.clear();
    
    std::cout << "Server stopped" << std::endl;
}

void EpollServer::handleNewConnection() {
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    
    while (true) {
        int client_fd = accept(listen_fd_, (struct sockaddr*)&client_addr, &client_len);
        if (client_fd == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // 没有更多连接了
                break;
            } else {
                std::cerr << "Accept failed: " << strerror(errno) << std::endl;
                break;
            }
        }
        
        // 设置为非阻塞模式
        int flags = fcntl(client_fd, F_GETFL, 0);
        fcntl(client_fd, F_SETFL, flags | O_NONBLOCK);
        
        // 添加到epoll
        uint32_t events = EPOLLIN | (config_.use_et_mode ? EPOLLET : 0);
        addEpollEvent(client_fd, events);
        
        // 初始化客户端缓冲区
        client_buffers_.insert(client_fd);
        
        // std::cout << "New client connected: " << inet_ntoa(client_addr.sin_addr) 
        //           << ":" << ntohs(client_addr.sin_port) << std::endl;
    }
}

void EpollServer::handleClientData(int fd) {
    std::string received_data;
    int msg_len = 0;
    // std::cout << "handle data" << std::endl;
    while ((msg_len = readCompleteMessage(fd, received_data)) > 0) {
        // 回射数据
        if (sendCompleteMessage(fd, received_data)) {
            // std::cout << "Echoed " << received_data.length() << " bytes to client " << fd << std::endl;
        } else {
            std::cerr << "Failed to send echo to client " << fd << std::endl;
            handleClientClose(fd);
        }
    } 
    if (msg_len < 0) {
        // 读取失败或连接关闭
        handleClientClose(fd);
    }
}

void EpollServer::handleClientClose(int fd) {
    removeEpollEvent(fd);
    close(fd);
    client_buffers_.erase(fd);
    // std::cout << "Client " << fd << " disconnected" << std::endl;
}

int EpollServer::readCompleteMessage(int fd, std::string& message) {
    // 读取消息头（长度字段）
    int msg_length;
    ssize_t bytes_received = recv(fd, &msg_length, sizeof(msg_length), 0);
    
    if (bytes_received == 0) {
        // std::cerr << "Connection closed by client" << std::endl;
        return -1;
    } else if (bytes_received < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
          // 非阻塞模式下没有数据可读
          return 0;
        }
        std::cerr << "Receive message header failed: " << strerror(errno) << std::endl;
        return -1;
    } else if (bytes_received != sizeof(msg_length)) {
        std::cerr << "Incomplete message header received" << std::endl;
        return -1;
    }
    
    // 转换为主机字节序
    msg_length = ntohl(msg_length);
    
    if (msg_length <= 0) {
        std::cerr << "Invalid message length: " << msg_length << std::endl;
        return -1;
    }
    
    // 读取消息体
    std::vector<char> buffer(msg_length);
    int bytes_cnt = 0;
    while(bytes_cnt < msg_length){
      bytes_received = recv(fd, buffer.data(), msg_length, 0);
      if (bytes_received < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
          // 非阻塞模式下没有数据可读
          continue;
        }
        std::cerr << "Read message body failed: " << strerror(errno) << std::endl;
        return -1;
      }
      bytes_cnt += bytes_received;
    }
    // total_recv += msg_length;
    // std::cout << total_recv << std::endl;
    message.assign(buffer.data(), msg_length);
    return msg_length;
}

bool EpollServer::sendCompleteMessage(int fd, const std::string& message) {
    // 计算整个结构体的大小
    size_t total_size = sizeof(int) + message.length();
    
    // 分配内存来构建完整的结构体
    std::vector<char> buffer(total_size);
    
    // 设置长度字段（网络字节序）
    int msg_length = htonl(message.length());
    memcpy(buffer.data(), &msg_length, sizeof(msg_length));
    
    // 拷贝数据内容
    memcpy(buffer.data() + sizeof(msg_length), message.data(), message.length());
    
    // 一次性发送整个结构体
    ssize_t bytes_sent = send(fd, buffer.data(), total_size, 0);
    if (bytes_sent != static_cast<ssize_t>(total_size)) {
        std::cerr << "Send message struct failed: " << strerror(errno) << std::endl;
        return false;
    }
    // total_send += message.length();
    // std::cout << total_send << std::endl;
    return true;
}

void EpollServer::addEpollEvent(int fd, uint32_t events) {
    struct epoll_event ev;
    ev.events = events;
    ev.data.fd = fd;
    
    if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &ev) == -1) {
        std::cerr << "Add epoll event failed for fd " << fd << ": " << strerror(errno) << std::endl;
    }
}

void EpollServer::modifyEpollEvent(int fd, uint32_t events) {
    struct epoll_event ev;
    ev.events = events;
    ev.data.fd = fd;
    
    if (epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, fd, &ev) == -1) {
        std::cerr << "Modify epoll event failed for fd " << fd << ": " << strerror(errno) << std::endl;
    }
}

void EpollServer::removeEpollEvent(int fd) {
    if (epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr) == -1) {
        std::cerr << "Remove epoll event failed for fd " << fd << ": " << strerror(errno) << std::endl;
    }
}