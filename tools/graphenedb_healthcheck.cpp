#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>
#include <iostream>
#include <string>

int main(int argc, char** argv) {
  const char* host = argc > 1 ? argv[1] : "127.0.0.1";
  int port = argc > 2 ? std::stoi(argv[2]) : 8080;
  int fd = ::socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) return 1;
  timeval timeout{3, 0};
  setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
  setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(static_cast<uint16_t>(port));
  if (inet_pton(AF_INET, host, &addr.sin_addr) != 1 ||
      ::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
    ::close(fd);
    return 1;
  }
  const std::string req = "GET /v1/health HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n";
  if (::write(fd, req.data(), req.size()) != static_cast<ssize_t>(req.size())) {
    ::close(fd);
    return 1;
  }
  char buf[512]{};
  ssize_t n = ::read(fd, buf, sizeof(buf) - 1);
  ::close(fd);
  if (n <= 0) return 1;
  std::string response(buf, static_cast<size_t>(n));
  return response.find("HTTP/1.1 200") == 0 ? 0 : 1;
}
