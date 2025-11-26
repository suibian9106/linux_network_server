#include "stress_client.h"
#include <iostream>
#include <chrono>
#include <sstream>
#include <iomanip>

StressClient::StressClient(const StressConfig& config) 
    : config_(config), gen_(rd_()) {
}

StressClient::~StressClient() {
    stop();
}

void StressClient::run() {
    if (running_) {
        std::cout << "Stress test is already running!" << std::endl;
        return;
    }
    
    running_ = true;
    stats_.reset();
    test_start_time_ = std::chrono::steady_clock::now();
    
    std::cout << "Starting stress test with " << config_.num_clients << " clients" << std::endl;
    if (config_.continuous_mode) {
        if (config_.duration_seconds > 0) {
            std::cout << "Duration: " << config_.duration_seconds << " seconds" << std::endl;
        } else {
            std::cout << "Duration: continuous until stopped" << std::endl;
        }
    } else {
        std::cout << "Requests per client: " << config_.requests_per_client << std::endl;
    }
    std::cout << "Server: " << config_.server_ip << ":" << config_.server_port << std::endl;
    if (config_.think_time_ms > 0) {
        std::cout << "Think time: " << config_.think_time_ms << " ms" << std::endl;
    }
    std::cout << "==================================" << std::endl;
    
    // 启动统计报告线程
    // reporter_thread_ = std::thread(&StressClient::statsReporter, this);
    
    // 创建工作线程
    for (int i = 0; i < config_.num_clients; ++i) {
        workers_.emplace_back(&StressClient::workerThread, this, i);
    }
    
    // 等待所有工作线程完成
    for (auto& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    
    // 停止统计报告线程
    running_ = false;
    // if (reporter_thread_.joinable()) {
    //     reporter_thread_.join();
    // }
    
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - test_start_time_);
    
    std::cout << "\n=== Stress Test Completed ===" << std::endl;
    printStats();
    
    double total_seconds = duration.count() / 1000.0;
    double requests_per_second = stats_.total_requests / total_seconds;
    double mb_sent = stats_.total_bytes_sent / (1024.0 * 1024.0);
    double mb_received = stats_.total_bytes_received / (1024.0 * 1024.0);
    double mb_per_second = mb_received / total_seconds;
    
    std::cout << "Total time: " << std::fixed << std::setprecision(2) << total_seconds << " seconds" << std::endl;
    std::cout << "Total connections: " << config_.num_clients << std::endl;
    std::cout << "Requests per second: " << requests_per_second << std::endl;
    std::cout << "Data sent: " << mb_sent << " MB" << std::endl;
    std::cout << "Data received: " << mb_received << " MB" << std::endl;
    std::cout << "Data per second: " << mb_per_second << " MB/s" << std::endl;
}

void StressClient::stop() {
    running_ = false;
    
    for (auto& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    workers_.clear();
    
    if (reporter_thread_.joinable()) {
        reporter_thread_.join();
    }
}

void StressClient::workerThread(int thread_id) {
    std::string client_name = "Client-" + std::to_string(thread_id);
    
    if (config_.verbose) {
        std::cout << client_name << " started" << std::endl;
    }
    
    // 创建客户端实例并连接
    Client client(config_.server_ip, config_.server_port);
    
    if (!client.connectToServer()) {
        std::cerr << client_name << " failed to connect to server" << std::endl;
        stats_.total_requests++;
        return;
    }
    
    int request_count = 0;
    
    // 持续运行或固定请求数运行
    while (running_ && shouldContinue()) {
        std::string message;
        
        if (config_.random_messages) {
            message = generateMessage();
        } else {
            message = client_name + " - Message " + std::to_string(request_count);
        }
        
        int sent_bytes = client.sendRequest(message);
        int received_bytes = client.receiveResponse();
        
        updateStats(sent_bytes, received_bytes);
        
        request_count++;
        
        // 检查是否达到固定请求数（非持续模式）
        if (!config_.continuous_mode && request_count >= config_.requests_per_client) {
            break;
        }
        
        // 思考时间
        if (config_.think_time_ms > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(config_.think_time_ms));
        }
    }
    
    // 断开连接
    client.disconnect();
    
    if (config_.verbose) {
        std::cout << client_name << " completed after " << request_count << " requests" << std::endl;
    }
}

void StressClient::statsReporter() {
    auto last_report_time = std::chrono::steady_clock::now();
    
    while (running_) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        auto current_time = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            current_time - last_report_time).count();
        
        if (elapsed >= config_.stats_interval) {
            printCurrentStats();
            last_report_time = current_time;
        }
    }
}

bool StressClient::shouldContinue() {
    if (!config_.continuous_mode) {
        return true; // 非持续模式由请求数控制
    }
    
    if (config_.duration_seconds > 0) {
        auto current_time = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            current_time - test_start_time_).count();
        return elapsed < config_.duration_seconds;
    }
    
    return true; // 无限运行
}

std::string StressClient::generateMessage() {
    static const char alphanum[] = 
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";
    
    std::string message;
    message.reserve(config_.message_size);
    
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, sizeof(alphanum) - 2);
    
    for (int i = 0; i < config_.message_size; ++i) {
        message += alphanum[dis(gen)];
    }
    
    return message;
}

void StressClient::updateStats(long sent_bytes, long received_bytes) {
    stats_.total_requests++;
    stats_.total_bytes_sent += sent_bytes;
    stats_.total_bytes_received += received_bytes;
}

void StressClient::printStats() const {
    std::cout << "=== Stress Test Statistics ===" << std::endl;
    std::cout << "Total requests: " << stats_.total_requests << std::endl;
    std::cout << "Total bytes sent: " << stats_.total_bytes_sent << std::endl;
    std::cout << "Total bytes received: " << stats_.total_bytes_received << std::endl;
}

void StressClient::printCurrentStats() {
    auto current_time = std::chrono::steady_clock::now();
    auto total_elapsed = std::chrono::duration_cast<std::chrono::seconds>(
        current_time - test_start_time_).count();
    
    double requests_per_second = total_elapsed > 0 ? 
        static_cast<double>(stats_.total_requests) / total_elapsed : 0;
    
    std::cout << "[Progress] Time: " << total_elapsed << "s, " 
              << "Requests: " << stats_.total_requests << ", "
              << "RPS: " << std::fixed << std::setprecision(2) << requests_per_second << ", "
              << std::endl;
}