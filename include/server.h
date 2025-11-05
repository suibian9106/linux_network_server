#ifndef EPOLL_SERVER_H
#define EPOLL_SERVER_H

#include <string>
#include <set>

// 简单文本协议 - echo服务器使用原始字节流
struct EchoMessage {
    int length;              // 数据长度
    char data[];             // 柔性数组，数据内容
};

// 服务器配置
struct ServerConfig {
    int port = 8080;
    int max_events = 1024;
    int timeout_ms = 10000; // 10秒超时
    bool use_et_mode = true; // 使用边缘触发模式
    int buffer_size = 4096;  // 缓冲区大小
};

class EpollServer {
public:
    EpollServer(const ServerConfig& config);
    ~EpollServer();
    
    bool initialize();
    void run();
    void stop();
    
private:
    bool setupListenSocket();       // 获取监听套接字
    bool setupEpoll();              // 创建epoll
    void handleNewConnection();     // 处理新连接
    void handleClientData(int fd);  // 处理客户端数据，回射
    void handleClientClose(int fd); // 关闭连接
    void addEpollEvent(int fd, uint32_t events);    // 添加epoll事件
    void modifyEpollEvent(int fd, uint32_t events); // 修改epoll事件
    void removeEpollEvent(int fd);                  // 删除epoll事件
    
    // 读取完整报文
    bool readCompleteMessage(int fd, std::string& message);
    // 发送完整报文
    bool sendCompleteMessage(int fd, const std::string& message);
    
private:
    ServerConfig config_;                   // 服务器配置
    int listen_fd_;                         // 监听套接字描述符
    int epoll_fd_;                          // epoll描述符
    bool running_;                          // 服务器是否在运行
    std::set<int> client_buffers_;          // 客户端
};

#endif // EPOLL_SERVER_H