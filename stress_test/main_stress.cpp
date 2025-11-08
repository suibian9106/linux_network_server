#include "stress_client.h"
#include <iostream>
#include <csignal>
#include <atomic>

std::atomic<bool> g_running{true};

void signalHandler(int signal) {
    std::cout << "\nReceived signal " << signal << ", stopping stress test..." << std::endl;
    g_running = false;
}

int main(int argc, char* argv[]) {
    // 注册信号处理
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    // 默认配置
    StressConfig config;
    
    // 简单的命令行参数解析
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-c" && i + 1 < argc) {
            config.num_clients = std::stoi(argv[++i]);
        } else if (arg == "-r" && i + 1 < argc) {
            config.requests_per_client = std::stoi(argv[++i]);
        } else if (arg == "-min" && i + 1 < argc) {
            config.message_min_size = std::stoi(argv[++i]);
        } else if (arg == "-max" && i + 1 < argc) {
            config.message_max_size = std::stoi(argv[++i]);
        } else if (arg == "-ip" && i + 1 < argc) {
            config.server_ip = argv[++i];
        } else if (arg == "-p" && i + 1 < argc) {
            config.server_port = std::stoi(argv[++i]);
        } else if (arg == "-v") {
            config.verbose = true;
        } else if (arg == "-h" || arg == "--help") {
            std::cout << "Usage: " << argv[0] << " [options]" << std::endl;
            std::cout << "Options:" << std::endl;
            std::cout << "  -c <num>      Number of concurrent clients (default: 10)" << std::endl;
            std::cout << "  -r <num>      Requests per client (default: 100)" << std::endl;
            std::cout << "  -min <size>   Minimum message size (default: 10)" << std::endl;
            std::cout << "  -max <size>   Maximum message size (default: 1024)" << std::endl;
            std::cout << "  -ip <addr>    Server IP address (default: 127.0.0.1)" << std::endl;
            std::cout << "  -p <port>     Server port (default: 8080)" << std::endl;
            std::cout << "  -v            Verbose output" << std::endl;
            std::cout << "  -h, --help    Show this help message" << std::endl;
            return 0;
        }
    }
    
    std::cout << "Echo Server Stress Test Generator" << std::endl;
    std::cout << "==================================" << std::endl;
    
    // 创建压力测试客户端
    StressClient stress_client(config);
    
    // 运行压力测试
    stress_client.run();
    
    return 0;
}