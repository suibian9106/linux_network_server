#include "../include/server.h"
#include <iostream>
#include <csignal>

EpollServer* g_server = nullptr;

void signalHandler(int signal) {
    std::cout << "\nReceived signal " << signal << ", shutting down server..." << std::endl;
    if (g_server) {
        g_server->stop();
    }
}

int main() {
    // 注册信号处理
    signal(SIGINT, signalHandler);
    // signal(SIGTERM, signalHandler);
    
    // 服务器配置
    ServerConfig config;
    config.port = 8080;
    config.max_events = 1024;
    config.timeout_ms = 10000;
    config.use_et_mode = true;
    
    // 创建服务器实例
    EpollServer server(config);
    g_server = &server;
    
    // 初始化服务器
    if (!server.initialize()) {
        std::cerr << "Server initialization failed" << std::endl;
        return 1;
    }
    
    // 运行服务器
    server.run();
    
    return 0;
}