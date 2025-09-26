#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <memory>
#include <netinet/in.h>
#include <signal.h>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <vector>

class TCPServer {
private:
  int server_fd;
  int port;
  bool running;

public:
  TCPServer(int port = 8080) : port(port), running(false), server_fd(-1) {}

  ~TCPServer() { stop(); }

  bool start() {
    // 创建socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
      std::cerr << "Failed to create socket" << std::endl;
      return false;
    }

    // 设置socket选项
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
      std::cerr << "Failed to set socket options" << std::endl;
      return false;
    }

    // 绑定地址
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    if (bind(server_fd, (sockaddr *)&address, sizeof(address)) < 0) {
      std::cerr << "Bind failed" << std::endl;
      return false;
    }

    // 开始监听
    if (listen(server_fd, 10) < 0) {
      std::cerr << "Listen failed" << std::endl;
      return false;
    }

    running = true;
    std::cout << "Server started on port " << port << std::endl;
    return true;
  }

  void stop() {
    if (running) {
      running = false;
      if (server_fd != -1) {
        close(server_fd);
        server_fd = -1;
      }
      std::cout << "Server stopped" << std::endl;
    }
  }

  void run() {
    if (!start()) {
      return;
    }

    while (running) {
      // 接受客户端连接
      sockaddr_in client_addr{};
      socklen_t client_len = sizeof(client_addr);
      int client_socket =
          accept(server_fd, (sockaddr *)&client_addr, &client_len);

      if (client_socket < 0) {
        if (running) {
          std::cerr << "Accept failed" << std::endl;
        }
        continue;
      }

      // 显示客户端信息
      char client_ip[INET_ADDRSTRLEN];
      inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
      std::cout << "Client connected: " << client_ip << ":"
                << ntohs(client_addr.sin_port) << std::endl;

      // 处理客户端请求
      handleClient(client_socket, client_ip, port);
    }
  }

private:
  void handleClient(int client_socket, char *client_ip, uint16_t port) {
    while (true) {
      char buffer[1024];

      // 读取客户端请求
      ssize_t bytes_read = read(client_socket, buffer, sizeof(buffer) - 1);
      if (bytes_read > 0) {
        buffer[bytes_read] = '\0';
        std::cout << "Received request:\n" << buffer << std::endl;
      }
      if (bytes_read == 0) {
        std::cout << "Client disconnect: " << client_ip << ":" << ntohs(port)
                  << std::endl;
        break;
      }

      // 发送HTTP响应
      std::string response =
          "HTTP/1.1 200 OK\r\n"
          "Content-Type: text/html\r\n"
          "Content-Length: 76\r\n"
          "\r\n"
          "<html><body><h1>Hello from C++ Server!</h1><p>This "
          "is a C++ implementation.</p></body></html>";

      write(client_socket, response.c_str(), response.length());
    }
  }
};

int main() {
  TCPServer server(8080);

  // 设置信号处理，优雅关闭
  signal(SIGINT, [](int sig) {
    std::cout << "\nShutting down server..." << std::endl;
    exit(0);
  });

  std::cout << "Starting C++ TCP Server..." << std::endl;
  std::cout << "Press Ctrl+C to stop the server." << std::endl;

  server.run();

  return 0;
}