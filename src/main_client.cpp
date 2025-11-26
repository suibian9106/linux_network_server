#include "../include/client.h"
#include <iostream>
#include <string>
#include <vector>
#include <unistd.h>

int main() {
    // 创建客户端
    Client client("127.0.0.1", 8080);
    
    // 连接服务器
    if (!client.connectToServer()) {
        std::cerr << "Failed to connect to server" << std::endl;
        return 1;
    }
    
    // 测试数据
    std::vector<std::string> test_messages = {
        "Hello, Echo Server!",
        "This is a test message.",
        "Another message to echo.",
        "Goodbye!"
    };
    
    // 发送测试消息
    for (const auto& message : test_messages) {
        if (client.sendRequest(message)<0) {
            std::cerr << "Request failed" << std::endl;
            break;
        }
        // 短暂延迟
        client.receiveResponse();
    }
    
    // 断开连接
    client.disconnect();
    
    return 0;
}