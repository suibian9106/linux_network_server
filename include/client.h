#ifndef CLIENT_H
#define CLIENT_H

#include <string>
#include <sys/socket.h>
#include <netinet/in.h>

// 简单文本协议 - echo服务器使用原始字节流
struct EchoMessage {
    int length;              // 数据长度
    char data[];             // 柔性数组，数据内容
};

class Client {
private:
    int sockfd;              // 套接字描述符
    std::string server_ip;   // 服务器IP
    int server_port;         // 服务器端口
    bool connected;          // 是否处于连接状态
    struct sockaddr_in server_addr; // 服务器地址

public:
    Client(const std::string &ip, int port);
    ~Client();
    bool connectToServer();  // 连接到服务器
    void disconnect();       // 关闭连接
    std::string sendRequest(const std::string &request, int timeout_seconds);  // 发送请求和处理接收
    bool isConnected() const; // 判断是否处于连接状态
    
private:
    bool setSocketTimeout(int timeout_seconds); // 设置socket超时
    bool sendCompleteMessage(const std::string& message); // 发送完整报文
    bool receiveCompleteMessage(std::string& message);    // 接收完整报文
};

#endif // CLIENT_H