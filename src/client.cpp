#include "../include/client.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <cerrno>
#include <vector>

Client::Client(const std::string &ip, int port) 
    : sockfd(-1), server_ip(ip), server_port(port), connected(false) {
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = inet_addr(ip.c_str());
}

Client::~Client() {
    disconnect();
}

bool Client::connectToServer() {
    // 创建socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        std::cerr << "Create socket failed: " << strerror(errno) << std::endl;
        return false;
    }
    
    // 连接服务器
    if (connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        std::cerr << "Connect to server failed: " << strerror(errno) << std::endl;
        close(sockfd);
        sockfd = -1;
        return false;
    }
    
    connected = true;
    // std::cout << "Connected to server " << server_ip << ":" << server_port << std::endl;
    return true;
}

void Client::disconnect() {
    if (sockfd != -1) {
        close(sockfd);
        sockfd = -1;
    }
    connected = false;
    // std::cout << "Disconnected from server" << std::endl;
}

bool Client::setSocketTimeout(int timeout_seconds) {
    struct timeval timeout;
    timeout.tv_sec = timeout_seconds;
    timeout.tv_usec = 0;
    
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        std::cerr << "Set receive timeout failed: " << strerror(errno) << std::endl;
        return false;
    }
    
    if (setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) < 0) {
        std::cerr << "Set send timeout failed: " << strerror(errno) << std::endl;
        return false;
    }
    
    return true;
}

std::string Client::sendRequest(const std::string &request, int timeout_seconds) {
    if (!connected) {
        std::cerr << "Not connected to server" << std::endl;
        return "";
    }
    
    // 设置超时
    if (!setSocketTimeout(timeout_seconds)) {
        return "";
    }
    
    // 发送请求
    if (!sendCompleteMessage(request)) {
        std::cerr << "Send request failed" << std::endl;
        connected = false;
        return "";
    }
    
    // std::cout << "Sent request: " << request << " (" << request.length() << " bytes)" << std::endl;
    
    // 接收响应
    std::string response;
    if (!receiveCompleteMessage(response)) {
        std::cerr << "Receive response failed" << std::endl;
        connected = false;
        return "";
    }
    
    // std::cout << "Received response: " << response << " (" << response.length() << " bytes)" << std::endl;
    return response;
}

bool Client::sendCompleteMessage(const std::string& message) {
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
    ssize_t bytes_sent = send(sockfd, buffer.data(), total_size, 0);
    if (bytes_sent != static_cast<ssize_t>(total_size)) {
        std::cerr << "Send message struct failed: " << strerror(errno) << std::endl;
        return false;
    }
    
    return true;
}

bool Client::receiveCompleteMessage(std::string& message) {
    // 读取消息头（长度字段）
    int msg_length;
    ssize_t bytes_received = recv(sockfd, &msg_length, sizeof(msg_length), 0);
    
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
      bytes_received = recv(sockfd, buffer.data(), msg_length, 0);
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
    
    message.assign(buffer.data(), msg_length);
    return true;
}

bool Client::isConnected() const {
    return connected;
}