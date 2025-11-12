#ifndef STRESS_CLIENT_H
#define STRESS_CLIENT_H

#include "../include/client.h"
#include <atomic>
#include <thread>
#include <vector>
#include <random>

// 压力测试统计
struct StressStats {
    std::atomic<long> total_requests{0};
    std::atomic<long> successful_requests{0};
    std::atomic<long> failed_requests{0};
    std::atomic<long> total_bytes_sent{0};
    std::atomic<long> total_bytes_received{0};

    void reset() {
        total_requests = 0;
        successful_requests = 0;
        failed_requests = 0;
        total_bytes_sent = 0;
        total_bytes_received = 0;
    }
};

// 压力测试配置
struct StressConfig {
    int num_clients = 10;           // 并发客户端数量
    int requests_per_client = 100;  // 每个客户端发送的请求数
    int message_min_size = 10;      // 消息最小大小
    int message_max_size = 1024;    // 消息最大大小
    int connect_timeout = 5;        // 连接超时(秒)
    int request_timeout = 3;        // 请求超时(秒)
    std::string server_ip = "127.0.0.1";
    int server_port = 8080;
    bool random_messages = true;    // 是否使用随机消息
    bool verbose = false;           // 是否输出详细信息
};

class StressClient {
public:
    StressClient(const StressConfig& config);
    ~StressClient();
    
    void run();                     // 运行压力测试
    void stop();                    // 停止压力测试
    void printStats() const;        // 打印统计信息
    
private:
    void workerThread(int thread_id, int num_clients);                   // 工作线程
    std::string generateRandomMessage(int min_size, int max_size); // 生成随机消息
    void updateStats(bool success, long sent_bytes, long received_bytes); // 更新统计
    
private:
    StressConfig config_;
    StressStats stats_;
    std::atomic<bool> running_{false};
    std::vector<std::thread> workers_;
    std::random_device rd_;
    std::mt19937 gen_;
};

#endif // STRESS_CLIENT_H