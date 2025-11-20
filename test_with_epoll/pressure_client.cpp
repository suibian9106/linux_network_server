#include "pressure_client.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <iostream>
#include <cerrno>
#include <random>
#include <thread>
#include <iomanip>

PressureClient::PressureClient(const ClientConfig& config) 
    : config_(config), epoll_fd_(-1), running_(false) {
}

PressureClient::~PressureClient() {
    stopTest();
}

bool PressureClient::initialize() {
    if (!setupEpoll()) {
        std::cerr << "Failed to setup epoll" << std::endl;
        return false;
    }
    
    // std::cout << "Pressure client initialized" << std::endl;
    // std::cout << "Target: " << config_.server_ip << ":" << config_.server_port << std::endl;
    // std::cout << "Messages per connection: " << config_.messages_per_connection << std::endl;
    // std::cout << "Message size: " << config_.message_size << " bytes" << std::endl;
    
    return true;
}

bool PressureClient::setupEpoll() {
    epoll_fd_ = epoll_create1(0);
    if (epoll_fd_ == -1) {
        std::cerr << "Create epoll failed: " << strerror(errno) << std::endl;
        return false;
    }
    return true;
}

void PressureClient::runTest() {
    if (epoll_fd_ == -1) {
        std::cerr << "Client not initialized" << std::endl;
        return;
    }
    
    running_ = true;
    stats_.start_time = std::chrono::steady_clock::now();
    
    std::cout << "Starting pressure test..." << std::endl;
    
    struct epoll_event events[config_.concurrent_connections];
    
    while (running_) {
        // 建立新连接直到达到并发数
        while (connections_.size() < config_.concurrent_connections) {
            Connection conn;
            conn.messages_to_send = config_.messages_per_connection;
            conn.connect_time = std::chrono::steady_clock::now();
            conn.last_activity = conn.connect_time;
            
            if (createConnection(conn)) {
                connections_[conn.fd] = conn;
                stats_.total_connections++;
            } else {
                stats_.failed_connections++;
            }
        }
        
        // 处理epoll事件
        int num_events = epoll_wait(epoll_fd_, events, config_.concurrent_connections, 100);
        
        if (num_events == -1) {
            if (errno == EINTR) {
                continue;
            }
            std::cerr << "Epoll wait failed: " << strerror(errno) << std::endl;
            break;
        }
        
        for (int i = 0; i < num_events; ++i) {
            int fd = events[i].data.fd;
            uint32_t event_type = events[i].events;
            
            auto it = connections_.find(fd);
            if (it == connections_.end()) {
                continue;
            }
            
            Connection& conn = it->second;
            
            if (event_type & (EPOLLERR | EPOLLHUP)) {
                handleClose(conn);
                continue;
            }
            
            switch (conn.state) {
                case CONNECTING:
                    if (event_type & EPOLLOUT) {
                        handleConnect(conn);
                    }
                    break;
                case CONNECTED:
                case SENDING:
                    if (event_type & EPOLLOUT) {
                        handleSend(conn);
                    }
                    break;
                case RECEIVING:
                    if (event_type & EPOLLIN) {
                        handleReceive(conn);
                    }
                    break;
                default:
                    break;
            }
        }
        
        // // 检查超时
        // checkTimeouts();
        
        // 检查测试是否完成
        auto now = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - stats_.start_time);
        if (duration.count() >= config_.test_duration) {
            std::cout << "Test duration reached, stopping..." << std::endl;
            break;
        }
        
        if (connections_.empty()) {
            std::cout << "All connections completed, stopping..." << std::endl;
            break;
        }
    }
    
    stats_.end_time = std::chrono::steady_clock::now();
    running_ = false;
    
    printStats();
}

void PressureClient::stopTest() {
    running_ = false;
    
    // 关闭所有连接
    for (auto& pair : connections_) {
        close(pair.first);
    }
    connections_.clear();
    
    if (epoll_fd_ != -1) {
        close(epoll_fd_);
        epoll_fd_ = -1;
    }
}

bool PressureClient::createConnection(Connection& conn) {
    conn.fd = socket(AF_INET, SOCK_STREAM, 0);
    if (conn.fd == -1) {
        std::cerr << "Create socket failed: " << strerror(errno) << std::endl;
        return false;
    }
    
    // 设置为非阻塞
    int flags = fcntl(conn.fd, F_GETFL, 0);
    fcntl(conn.fd, F_SETFL, flags | O_NONBLOCK);
    
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(config_.server_port);
    inet_pton(AF_INET, config_.server_ip.c_str(), &server_addr.sin_addr);
    
    int ret = connect(conn.fd, (struct sockaddr*)&server_addr, sizeof(server_addr));
    if (ret == -1) {
        if (errno == EINPROGRESS) {
            // 连接进行中
            uint32_t events = EPOLLOUT | (config_.use_et_mode ? EPOLLET : 0);
            addEpollEvent(conn.fd, events);
            conn.state = CONNECTING;
            return true;
        } else {
            std::cerr << "Connect failed: " << strerror(errno) << std::endl;
            close(conn.fd);
            return false;
        }
    }
    
    // 立即连接成功
    conn.state = CONNECTED;
    uint32_t events = EPOLLOUT | (config_.use_et_mode ? EPOLLET : 0);
    addEpollEvent(conn.fd, events);
    return true;
}

void PressureClient::handleConnect(Connection& conn) {
    int error = 0;
    socklen_t len = sizeof(error);
    if (getsockopt(conn.fd, SOL_SOCKET, SO_ERROR, &error, &len) < 0 || error != 0) {
        std::cerr << "Connection failed: " << strerror(error) << std::endl;
        handleClose(conn);
        return;
    }
    
    conn.state = CONNECTED;
    stats_.successful_connections++;
    
    // 准备发送第一条消息
    conn.send_buffer = generateMessage();
    modifyEpollEvent(conn.fd, EPOLLOUT | (config_.use_et_mode ? EPOLLET : 0));
}

void PressureClient::handleSend(Connection& conn) {
    if (!sendMessage(conn)) {
        handleClose(conn);
        return;
    }
    
    conn.last_activity = std::chrono::steady_clock::now();
    
    if (conn.messages_sent < conn.messages_to_send) {
        // 准备下一条消息
        conn.send_buffer = generateMessage();
        conn.state = SENDING;
    } else {
        // 所有消息发送完成，等待接收
        conn.state = RECEIVING;
        modifyEpollEvent(conn.fd, EPOLLIN | (config_.use_et_mode ? EPOLLET : 0));
    }
}

void PressureClient::handleReceive(Connection& conn) {
    if (!receiveMessage(conn)) {
        handleClose(conn);
        return;
    }
    
    conn.last_activity = std::chrono::steady_clock::now();
    
    if (conn.messages_received >= conn.messages_to_send) {
        // 所有消息接收完成
        handleClose(conn);
    }
}

void PressureClient::handleClose(Connection& conn) {
    if (conn.state != CONNECTING && conn.messages_received < conn.messages_to_send) {
        stats_.failed_connections++;
    }
    removeEpollEvent(conn.fd);
    close(conn.fd);
    connections_.erase(conn.fd);
}

bool PressureClient::sendMessage(Connection& conn) {
    std::string message=conn.send_buffer;

    size_t total_size = sizeof(int) + message.length();
    
    // 分配内存来构建完整的结构体
    std::vector<char> buffer(total_size);
    
    // 设置长度字段（网络字节序）
    int msg_length = htonl(message.length());
    memcpy(buffer.data(), &msg_length, sizeof(msg_length));
    
    // 拷贝数据内容
    memcpy(buffer.data() + sizeof(msg_length), message.data(), message.length());
    
    // 一次性发送整个结构体
    ssize_t bytes_sent = send(conn.fd, buffer.data(), total_size, 0);
    if (bytes_sent != static_cast<ssize_t>(total_size)) {
        std::cerr << "Send message struct failed: " << strerror(errno) << std::endl;
        return false;
    }
    
    conn.messages_sent++;
    stats_.messages_sent++;
    stats_.bytes_sent += bytes_sent;
    
    return true;
}

bool PressureClient::receiveMessage(Connection& conn) {
    // 读取消息头（长度字段）
    int msg_length;
    ssize_t bytes_received = recv(conn.fd, &msg_length, sizeof(msg_length), 0);
    
    if (bytes_received == 0) {
        std::cerr << "Connection closed by server" << std::endl;
        return false;
    } else if (bytes_received < 0) {
        std::cerr << "Receive message header failed: " << strerror(errno) << std::endl;
        return false;
    } else if (bytes_received != sizeof(msg_length)) {
        std::cerr << "Incomplete message header received" << std::endl;
        return false;
    }
    
    // 转换为主机字节序
    msg_length = ntohl(msg_length);
    
    if (msg_length <= 0 || msg_length > 1024 * 1024) { // 限制最大1MB
        std::cerr << "Invalid message length: " << msg_length << std::endl;
        return false;
    }
    
    // 读取消息体
    std::vector<char> buffer(msg_length);
    int bytes_cnt = 0;
    while(bytes_cnt < msg_length){
      bytes_received = recv(conn.fd, buffer.data(), msg_length, 0);
      if (bytes_received < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
          // 非阻塞模式下没有数据可读
          continue;
        }
        std::cerr << "Read message body failed: " << strerror(errno) << std::endl;
        return false;
      }
      bytes_cnt += bytes_received;
    }
    conn.receive_buffer.assign(buffer.data(), msg_length);
    
    // 验证回射数据
    if (conn.receive_buffer != conn.send_buffer) {
        std::cerr << "Echo data mismatch!" << std::endl;
    }
    conn.messages_received++;
    stats_.messages_received++;
    stats_.bytes_received += bytes_cnt;
    return true;
}

// void PressureClient::checkTimeouts() {
//     auto now = std::chrono::steady_clock::now();
//     std::vector<int> timeouts;
    
//     for (auto& pair : connections_) {
//         auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
//             now - pair.second.last_activity);
//         if (duration.count() > config_.timeout_ms) {
//             timeouts.push_back(pair.first);
//             stats_.timeouts++;
//         }
//     }
    
//     for (int fd : timeouts) {
//         auto it = connections_.find(fd);
//         if (it != connections_.end()) {
//             handleClose(it->second);
//         }
//     }
// }

std::string PressureClient::generateMessage() {
    static const char alphanum[] = 
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";
    
    std::string message;
    message.reserve(config_.message_size);
    
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, sizeof(alphanum) - 2);
    
    for (int i = 0; i < config_.message_size; ++i) {
        message += alphanum[dis(gen)];
    }
    
    return message;
}

void PressureClient::addEpollEvent(int fd, uint32_t events) {
    struct epoll_event ev;
    ev.events = events;
    ev.data.fd = fd;
    
    if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &ev) == -1) {
        std::cerr << "Add epoll event failed: " << strerror(errno) << std::endl;
    }
}

void PressureClient::modifyEpollEvent(int fd, uint32_t events) {
    struct epoll_event ev;
    ev.events = events;
    ev.data.fd = fd;
    
    if (epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, fd, &ev) == -1) {
        std::cerr << "Modify epoll event failed: " << strerror(errno) << std::endl;
    }
}

void PressureClient::removeEpollEvent(int fd) {
    if (epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr) == -1) {
        std::cerr << "Remove epoll event failed: " << strerror(errno) << std::endl;
    }
}

void PressureClient::printStats() {
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        stats_.end_time - stats_.start_time);
    double duration_sec = duration.count() / 1000.0;
    
    std::cout << "\n=== Pressure Test Results ===" << std::endl;
    std::cout << "Duration: " << duration_sec << " seconds" << std::endl;
    std::cout << "Total connections: " << stats_.total_connections << std::endl;
    std::cout << "Successful connections: " << stats_.successful_connections << std::endl;
    std::cout << "Failed connections: " << stats_.failed_connections << std::endl;
    std::cout << "Timeouts: " << stats_.timeouts << std::endl;
    std::cout << "Messages sent: " << stats_.messages_sent << std::endl;
    std::cout << "Messages received: " << stats_.messages_received << std::endl;
    std::cout << "Bytes sent: " << stats_.bytes_sent << std::endl;
    std::cout << "Bytes received: " << stats_.bytes_received << std::endl;
    
    if (duration_sec > 0) {
        std::cout << "Connections per second: " 
                  << stats_.total_connections / duration_sec << std::endl;
        std::cout << "Messages per second: " 
                  << stats_.messages_sent / duration_sec << std::endl;
        std::cout << "Throughput: " 
                  << (stats_.bytes_sent + stats_.bytes_received) / duration_sec / 1024 
                  << " KB/s" << std::endl;
    }
    
    double success_rate = (stats_.total_connections > 0) ? 
        (static_cast<double>(stats_.successful_connections) / stats_.total_connections * 100) : 0;
    std::cout << "Success rate: " << std::fixed << std::setprecision(2) 
              << success_rate << "%" << std::endl;
}