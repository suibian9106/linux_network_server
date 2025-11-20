#ifndef PRESSURE_CLIENT_H
#define PRESSURE_CLIENT_H

#include <string>
#include <vector>
#include <atomic>
#include <chrono>
#include <map>

struct ClientConfig {
    std::string server_ip = "127.0.0.1";
    int server_port = 8080;
    int concurrent_connections = 1000;  // 并发连接数
    int messages_per_connection = 10;  // 每个连接发送的消息数
    int message_size = 1024;           // 每条消息大小(字节)
    int timeout_ms = 5000;             // 超时时间
    bool use_et_mode = true;           // 使用边缘触发
    int batch_size = 10;               // 批量连接数
    int test_duration = 30;            // 测试持续时间(秒)
};

struct TestStats {
    std::atomic<long> total_connections{0};
    std::atomic<long> successful_connections{0};
    std::atomic<long> failed_connections{0};
    std::atomic<long> messages_sent{0};
    std::atomic<long> messages_received{0};
    std::atomic<long> bytes_sent{0};
    std::atomic<long> bytes_received{0};
    std::atomic<long> timeouts{0};
    std::chrono::steady_clock::time_point start_time;
    std::chrono::steady_clock::time_point end_time;
};

class PressureClient {
public:
    PressureClient(const ClientConfig& config);
    ~PressureClient();
    
    bool initialize();
    void runTest();
    void stopTest();
    void printStats();
    
private:
    enum ConnectionState {
        CONNECTING,
        CONNECTED,
        SENDING,
        RECEIVING,
        CLOSING,
        CLOSED
    };
    
    struct Connection {
        int fd = -1;
        ConnectionState state = CONNECTING;
        int messages_to_send = 0;
        int messages_sent = 0;
        int messages_received = 0;
        std::string send_buffer;
        std::string receive_buffer;
        int expected_length = 0;
        std::chrono::steady_clock::time_point connect_time;
        std::chrono::steady_clock::time_point last_activity;
    };
    
    bool createConnection(Connection& conn);
    bool setupEpoll();
    void addEpollEvent(int fd, uint32_t events);
    void modifyEpollEvent(int fd, uint32_t events);
    void removeEpollEvent(int fd);

    void handleConnect(Connection& conn);
    void handleSend(Connection& conn);
    void handleReceive(Connection& conn);
    void handleClose(Connection& conn);
    // void checkTimeouts();
    
    bool sendMessage(Connection& conn);
    bool receiveMessage(Connection& conn);
    
    std::string generateMessage();
    
private:
    ClientConfig config_;
    int epoll_fd_;
    bool running_;
    TestStats stats_;
    std::map<int, Connection> connections_;
};

#endif // PRESSURE_CLIENT_H