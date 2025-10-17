#include <arpa/inet.h>
#include <chrono>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

class TCPClient {
private:
  int sockfd;
  std::string server_ip;
  int server_port;
  bool connected;

public:
  TCPClient(const std::string &ip = "127.0.0.1", int port = 8080)
      : server_ip(ip), server_port(port), connected(false), sockfd(-1) {}

  ~TCPClient() { disconnect(); }

  bool connectToServer() {
    // 创建socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
      std::cerr << "Failed to create socket" << std::endl;
      return false;
    }

    // 设置服务器地址
    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);

    if (inet_pton(AF_INET, server_ip.c_str(), &server_addr.sin_addr) <= 0) {
      std::cerr << "Invalid address or address not supported" << std::endl;
      return false;
    }

    // 连接服务器
    if (connect(sockfd, (sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
      std::cerr << "Connection failed to " << server_ip << ":" << server_port
                << std::endl;
      return false;
    }

    connected = true;
    std::cout << "Connected to server " << server_ip << ":" << server_port
              << std::endl;
    return true;
  }

  void disconnect() {
    if (connected) {
      close(sockfd);
      connected = false;
      std::cout << "Disconnected from server" << std::endl;
    }
  }

  std::string sendRequest(const std::string &request, int timeout_seconds = 5) {
    if (!connected) {
      if (!connectToServer()) {
        return "Connection failed";
      }
    }

    // 设置超时
    struct timeval timeout;
    timeout.tv_sec = timeout_seconds;
    timeout.tv_usec = 0;

    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

    // 发送请求
    ssize_t bytes_sent = send(sockfd, request.c_str(), request.length(), 0);
    if (bytes_sent < 0) {
      return "Send failed";
    }

    std::cout << "Sent " << bytes_sent << " bytes to server" << std::endl;



    // 接收响应
    char buffer[4096];
    std::string response;

    while (true) {
      ssize_t bytes_received = recv(sockfd, buffer, sizeof(buffer) - 1, 0);
      if (bytes_received < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
          std::cout << "Receive timeout" << std::endl;
          break;
        }
        return "Receive failed";
      } else if (bytes_received == 0) {
        std::cout << "Server closed connection" << std::endl;
        break;
      } else {
        buffer[bytes_received] = '\0';
        response.append(buffer, bytes_received);
        break;
      }
    }

    return response;
  }

  bool isConnected() const { return connected; }
};

int main() {
  std::cout << "=== C++ TCP Client ===" << std::endl;

  TCPClient client("127.0.0.1", 8080);

  // 测试请求
  std::string request;
  std::cout<<"input request context:"<<std::endl;
  std::cin>>request;

  auto start = std::chrono::high_resolution_clock::now();
  std::string response = client.sendRequest(request);
  auto end = std::chrono::high_resolution_clock::now();

  auto duration =
      std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

  std::cout << "Response received in " << duration.count() << "ms" << std::endl;
  std::cout << "Response:\n" << response << std::endl;

  // 等待一会儿再发送下一个请求
  std::this_thread::sleep_for(std::chrono::seconds(1));

  // 测试请求
  std::cout<<"input request context:"<<std::endl;
  std::cin>>request;

  start = std::chrono::high_resolution_clock::now();
  response = client.sendRequest(request);
  end = std::chrono::high_resolution_clock::now();

  duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

  std::cout << "Response received in " << duration.count() << "ms" << std::endl;
  std::cout << "Response:\n" << response << std::endl;

  return 0;
}