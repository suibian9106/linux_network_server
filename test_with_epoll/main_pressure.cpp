#include "pressure_client.h"
#include <iostream>
#include <csignal>
#include <cstdlib>

PressureClient* g_client = nullptr;

void signalHandler(int signal) {
    std::cout << "\nReceived signal " << signal << ", stopping test..." << std::endl;
    if (g_client) {
        g_client->stopTest();
    }
    exit(0);
}

void printUsage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [options]" << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  -h HOST        Server IP (default: 127.0.0.1)" << std::endl;
    std::cout << "  -p PORT        Server port (default: 8080)" << std::endl;
    std::cout << "  -c CONNECTIONS Total connections (default: 1000)" << std::endl;
    std::cout << "  -n CONCURRENT Concurrent connections (default: 100)" << std::endl;
    std::cout << "  -m MESSAGES    Messages per connection (default: 10)" << std::endl;
    std::cout << "  -s SIZE        Message size in bytes (default: 1024)" << std::endl;
    std::cout << "  -t SECONDS     Test duration in seconds (default: 30)" << std::endl;
    std::cout << "  --help         Show this help message" << std::endl;
}

int main(int argc, char* argv[]) {
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    ClientConfig config;
    
    // 解析命令行参数
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" && i + 1 < argc) {
            config.server_ip = argv[++i];
        } else if (arg == "-p" && i + 1 < argc) {
            config.server_port = std::atoi(argv[++i]);
        } else if (arg == "-c" && i + 1 < argc) {
            config.concurrent_connections = std::atoi(argv[++i]);
        } else if (arg == "-m" && i + 1 < argc) {
            config.messages_per_connection = std::atoi(argv[++i]);
        } else if (arg == "-s" && i + 1 < argc) {
            config.message_size = std::atoi(argv[++i]);
        } else if (arg == "-t" && i + 1 < argc) {
            config.test_duration = std::atoi(argv[++i]);
        } else if (arg == "--help") {
            printUsage(argv[0]);
            return 0;
        } else {
            std::cerr << "Unknown option: " << arg << std::endl;
            printUsage(argv[0]);
            return 1;
        }
    }
    std::cout << "Starting pressure test with configuration:" << std::endl;
    std::cout << "  Server: " << config.server_ip << ":" << config.server_port << std::endl;
    std::cout << "  Concurrent connections: " << config.concurrent_connections << std::endl;
    std::cout << "  Messages per connection: " << config.messages_per_connection << std::endl;
    std::cout << "  Message size: " << config.message_size << " bytes" << std::endl;
    std::cout << "  Test duration: " << config.test_duration << " seconds" << std::endl;
    
    PressureClient client(config);
    g_client = &client;
    
    if (!client.initialize()) {
        std::cerr << "Failed to initialize pressure client" << std::endl;
        return 1;
    }
    
    client.runTest();
    
    return 0;
}