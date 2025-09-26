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

        // 检查是否接收完整（简单的HTTP响应检查）
        if (response.find("\r\n\r\n") != std::string::npos) {
          size_t content_length_pos = response.find("Content-Length: ");
          if (content_length_pos != std::string::npos) {
            size_t content_start = response.find("\r\n\r\n") + 4;
            size_t content_length =
                std::stoi(response.substr(content_length_pos + 16));
            if (response.length() - content_start >= content_length) {
              break;
            }
          } else {
            // 没有Content-Length，认为接收完成
            break;
          }
        }
      }
    }

    return response;
  }

  bool isConnected() const { return connected; }
};

// HTTP请求构建器
class HTTPRequest {
public:
  static std::string createGET(const std::string &path = "/",
                               const std::string &host = "localhost") {
    return "GET " + path +
           " HTTP/1.1\r\n"
           "Host: " +
           host +
           "\r\n"
           "User-Agent: C++ TCP Client\r\n"
           "Connection: close\r\n"
           "\r\n";
  }

  static std::string createPOST(const std::string &path = "/",
                                const std::string &host = "localhost",
                                const std::string &data = "") {
    return "POST " + path +
           " HTTP/1.1\r\n"
           "Host: " +
           host +
           "\r\n"
           "User-Agent: C++ TCP Client\r\n"
           "Content-Type: application/x-www-form-urlencoded\r\n"
           "Content-Length: " +
           std::to_string(data.length()) +
           "\r\n"
           "Connection: close\r\n"
           "\r\n" +
           data;
  }
};

int main() {
  std::cout << "=== C++ TCP Client ===" << std::endl;

  TCPClient client("127.0.0.1", 8080);

  // 测试GET请求
  std::string get_request = HTTPRequest::createGET("/", "localhost");
  std::cout << "\nSending GET request..." << std::endl;

  auto start = std::chrono::high_resolution_clock::now();
  std::string response = client.sendRequest(get_request);
  auto end = std::chrono::high_resolution_clock::now();

  auto duration =
      std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

  std::cout << "Response received in " << duration.count() << "ms" << std::endl;
  std::cout << "Response:\n" << response << std::endl;

  // 等待一会儿再发送下一个请求
  std::this_thread::sleep_for(std::chrono::seconds(1));

  // 测试POST请求
  std::string post_data = "message=Hello+Server&client=cpp";
  std::string post_request =
      HTTPRequest::createPOST("/api", "localhost", post_data);
  std::cout << "\nSending POST request..." << std::endl;

  start = std::chrono::high_resolution_clock::now();
  response = client.sendRequest(post_request);
  end = std::chrono::high_resolution_clock::now();

  duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

  std::cout << "Response received in " << duration.count() << "ms" << std::endl;
  std::cout << "Response:\n" << response << std::endl;

  return 0;
}