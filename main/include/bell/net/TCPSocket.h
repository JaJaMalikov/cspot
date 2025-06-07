#pragma once
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <string>
#include "bell/Result.h"

namespace bell {
namespace net {
class TCPSocket {
 public:
  TCPSocket() : fd(-1) {}
  ~TCPSocket() { close(); }
  bell::Result<> connect(const std::string& host, int port, int /*timeoutMs*/ = 0) {
    struct addrinfo hints{}, *res;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &res) != 0)
      return std::errc::io_error;
    fd = socket(res->ai_family, res->ai_socktype, 0);
    if (fd < 0) {
      freeaddrinfo(res);
      return std::errc::io_error;
    }
    if (::connect(fd, res->ai_addr, res->ai_addrlen) != 0) {
      freeaddrinfo(res);
      return std::errc::io_error;
    }
    freeaddrinfo(res);
    return {};
  }
  bell::Result<size_t> read(uint8_t* buf, size_t len) {
    ssize_t r = ::read(fd, buf, len);
    if (r < 0) return std::errc::io_error;
    return static_cast<size_t>(r);
  }
  bell::Result<size_t> write(const uint8_t* buf, size_t len) {
    ssize_t w = ::write(fd, buf, len);
    if (w < 0) return std::errc::io_error;
    return static_cast<size_t>(w);
  }
  void close() {
    if (fd >= 0) {
      ::close(fd);
      fd = -1;
    }
  }
 private:
  int fd;
};
}  // namespace net
}  // namespace bell
