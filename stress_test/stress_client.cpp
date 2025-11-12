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
    stats_.reset(); // 重置统计
    
    std::cout << "Starting stress test with " << config_.num_clients 
              << " clients, " << config_.requests_per_client << " requests per client" << std::endl;
    std::cout << "Server: " << config_.server_ip << ":" << config_.server_port << std::endl;
    
    auto start_time = std::chrono::steady_clock::now();
    
    // 创建工作线程
    for (int i = 0; i < 100; ++i) {
        workers_.emplace_back(&StressClient::workerThread, this, i, config_.num_clients);
    }
    
    // 等待所有工作线程完成
    for (auto& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    std::cout << "\n=== Stress Test Completed ===" << std::endl;
    printStats();
    
    double total_seconds = duration.count() / 1000.0;
    double connection_per_second = config_.num_clients / total_seconds;
    double requests_per_second = stats_.total_requests / total_seconds;
    double mb_sent = stats_.total_bytes_sent / (1024.0 * 1024.0);
    double mb_received = stats_.total_bytes_received / (1024.0 * 1024.0);
    double mb_per_second = mb_received / total_seconds;
    
    std::cout << "Total time: " << total_seconds << " seconds" << std::endl;
    std::cout << "Total connections: " << config_.num_clients << std::endl;
    std::cout << "Connections per second: " << connection_per_second << std::endl;
    std::cout << "Requests per second: " << requests_per_second << std::endl;
    std::cout << "Data sent: " << mb_sent << " MB" << std::endl;
    std::cout << "Data received: " << mb_received << " MB" << std::endl;
    std::cout << "Data per second: " << mb_per_second << " MB/s" << std::endl;
    std::cout << "Success rate: " 
              << (static_cast<double>(stats_.successful_requests) / stats_.total_requests * 100.0)
              << "%" << std::endl;
}

void StressClient::stop() {
    running_ = false;
    
    for (auto& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    workers_.clear();
}

void StressClient::workerThread(int thread_id, int num_clients) {
    std::string client_name = "Client-" + std::to_string(thread_id);
    
    if (config_.verbose) {
        std::cout << client_name << " started" << std::endl;
    }
    for(int i = 0; i < num_clients / 100; ++i) {
        // 创建客户端实例
        Client client(config_.server_ip, config_.server_port);
        
        // 连接到服务器
        if (!client.connectToServer()) {
            std::cerr << client_name << " failed to connect to server" << std::endl;
            stats_.failed_requests += config_.requests_per_client;
            stats_.total_requests += config_.requests_per_client;
            return;
        }
        
        // 发送请求
        for (int i = 0; i < config_.requests_per_client && running_; ++i) {
            std::string message;
            
            if (config_.random_messages) {
                message = generateRandomMessage(config_.message_min_size, config_.message_max_size);
            } else {
                message = client_name + " - Message " + std::to_string(i);
            }
            
            long sent_bytes = message.length();
            std::string response = client.sendRequest(message, config_.request_timeout);
            long received_bytes = response.length();
            
            bool success = (!response.empty() && response == message);
            updateStats(success, sent_bytes, received_bytes);
            
            if (config_.verbose) {
                if (success) {
                    std::cout << client_name << " request " << i << " successful" << std::endl;
                } else {
                    std::cerr << client_name << " request " << i << " failed" << std::endl;
                }
            }
            
            // 短暂延迟，避免过度占用CPU
            // std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
            // 断开连接
        client.disconnect();
        if (config_.verbose) {
            std::cout << client_name << " completed" << std::endl;
        }
    }
}

std::string StressClient::generateRandomMessage(int min_size, int max_size) {
    static const char alphanum[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";
    
    std::uniform_int_distribution<> size_dist(min_size, max_size);
    std::uniform_int_distribution<> char_dist(0, sizeof(alphanum) - 2);
    
    int size = size_dist(gen_);
    std::string message;
    message.reserve(size);
    
    for (int i = 0; i < size; ++i) {
        message += alphanum[char_dist(gen_)];
    }
    
    return message;
}

void StressClient::updateStats(bool success, long sent_bytes, long received_bytes) {
    stats_.total_requests++;
    
    if (success) {
        stats_.successful_requests++;
        stats_.total_bytes_sent += sent_bytes;
        stats_.total_bytes_received += received_bytes;
    } else {
        stats_.failed_requests++;
    }
}

void StressClient::printStats() const {
    std::cout << "=== Stress Test Statistics ===" << std::endl;
    std::cout << "Total requests: " << stats_.total_requests << std::endl;
    std::cout << "Successful requests: " << stats_.successful_requests << std::endl;
    std::cout << "Failed requests: " << stats_.failed_requests << std::endl;
    // std::cout << "Total bytes sent: " << stats_.total_bytes_sent << std::endl;
    // std::cout << "Total bytes received: " << stats_.total_bytes_received << std::endl;
}