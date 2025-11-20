#include "stress_client.h"
#include <iostream>
#include <csignal>
#include <atomic>

std::atomic<bool> g_running{true};

void signalHandler(int signal) {
    std::cout << "\nReceived signal " << signal << ", stopping stress test..." << std::endl;
    g_running = false;
}

void printUsage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [options]" << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  -c <num>      Number of concurrent clients (default: 10)" << std::endl;
    std::cout << "  -r <num>      Requests per client (default: 100, ignored in continuous mode)" << std::endl;
    std::cout << "  -d <seconds>  Test duration in seconds (enables continuous mode)" << std::endl;
    std::cout << "  -cont         Continuous mode until stopped" << std::endl;
    std::cout << "  -min <size>   Minimum message size (default: 10)" << std::endl;
    std::cout << "  -max <size>   Maximum message size (default: 1024)" << std::endl;
    std::cout << "  -t <ms>       Think time between requests in milliseconds (default: 0)" << std::endl;
    std::cout << "  -ip <addr>    Server IP address (default: 127.0.0.1)" << std::endl;
    std::cout << "  -p <port>     Server port (default: 8080)" << std::endl;
    std::cout << "  -s <seconds>  Statistics report interval in seconds (default: 5)" << std::endl;
    std::cout << "  -v            Verbose output" << std::endl;
    std::cout << "  -h, --help    Show this help message" << std::endl;
    std::cout << std::endl;
    std::cout << "Examples:" << std::endl;
    std::cout << "  " << program_name << " -c 10 -r 1000 -ip 127.0.0.1 -p 8080" << std::endl;
    std::cout << "  " << program_name << " -c 50 -d 60 -t 100 -ip 192.168.1.100 -p 8080" << std::endl;
    std::cout << "  " << program_name << " -c 100 -cont -t 50 -s 10" << std::endl;
}

int main(int argc, char* argv[]) {
    // 注册信号处理
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    // 默认配置
    StressConfig config;
    
    // 命令行参数解析
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-c" && i + 1 < argc) {
            config.num_clients = std::stoi(argv[++i]);
        } else if (arg == "-r" && i + 1 < argc) {
            config.requests_per_client = std::stoi(argv[++i]);
        } else if (arg == "-d" && i + 1 < argc) {
            config.duration_seconds = std::stoi(argv[++i]);
            config.continuous_mode = true;
        } else if (arg == "-cont") {
            config.continuous_mode = true;
        } else if (arg == "-min" && i + 1 < argc) {
            config.message_min_size = std::stoi(argv[++i]);
        } else if (arg == "-max" && i + 1 < argc) {
            config.message_max_size = std::stoi(argv[++i]);
        } else if (arg == "-t" && i + 1 < argc) {
            config.think_time_ms = std::stoi(argv[++i]);
        } else if (arg == "-ip" && i + 1 < argc) {
            config.server_ip = argv[++i];
        } else if (arg == "-p" && i + 1 < argc) {
            config.server_port = std::stoi(argv[++i]);
        } else if (arg == "-s" && i + 1 < argc) {
            config.stats_interval = std::stoi(argv[++i]);
        } else if (arg == "-v") {
            config.verbose = true;
        } else if (arg == "-h" || arg == "--help") {
            printUsage(argv[0]);
            return 0;
        } else {
            std::cerr << "Unknown option: " << arg << std::endl;
            printUsage(argv[0]);
            return 1;
        }
    }
    
    std::cout << "Echo Server Stress Test Generator" << std::endl;
    std::cout << "==================================" << std::endl;
    
    try {
        // 创建压力测试客户端
        StressClient stress_client(config);
        
        // 运行压力测试
        stress_client.run();
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}