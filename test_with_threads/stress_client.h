#ifndef STRESS_CLIENT_H
#define STRESS_CLIENT_H

#include "../include/client.h"
#include <atomic>
#include <thread>
#include <vector>
#include <random>
#include <chrono>
#include <mutex>
#include <condition_variable>

// 压力测试统计
struct StressStats {
    std::atomic<long> total_requests{0};
    std::atomic<long> total_bytes_sent{0};
    std::atomic<long> total_bytes_received{0};

    void reset() {
        total_requests = 0;
        total_bytes_sent = 0;
        total_bytes_received = 0;
    }
};

// 压力测试配置
struct StressConfig {
    int num_clients = 10;           // 并发客户端数量
    int requests_per_client = 100;  // 每个客户端发送的请求数
    int message_size = 10;          // 消息大小
    int connect_timeout = 5;        // 连接超时(秒)
    int request_timeout = 3;        // 请求超时(秒)
    std::string server_ip = "127.0.0.1";
    int server_port = 8080;
    bool random_messages = true;    // 是否使用随机消息
    bool verbose = false;           // 是否输出详细信息
    bool continuous_mode = false;   // 是否持续运行模式
    int duration_seconds = 0;       // 测试持续时间(秒)，0表示无限
    int think_time_ms = 0;          // 思考时间(毫秒)
    int stats_interval = 5;         // 统计报告间隔(秒)
};

class StressClient {
public:
    StressClient(const StressConfig& config);
    ~StressClient();
    
    void run();                     // 运行压力测试
    void stop();                    // 停止压力测试
    void printStats() const;        // 打印统计信息
    
private:
    void workerThread(int thread_id);                   // 工作线程
    void statsReporter();                               // 统计报告线程
    std::string generateMessage();                      // 生成消息
    void updateStats(long sent_bytes, long received_bytes); // 更新统计
    bool shouldContinue();                              // 检查是否继续运行
    void printCurrentStats();                           // 打印当前统计
    
private:
    StressConfig config_;
    StressStats stats_;
    std::atomic<bool> running_{false};
    std::vector<std::thread> workers_;
    std::thread reporter_thread_;
    std::random_device rd_;
    std::mt19937 gen_;
    std::chrono::steady_clock::time_point test_start_time_;
};

#endif // STRESS_CLIENT_H