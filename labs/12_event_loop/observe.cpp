#include "lab_check.hpp"
#include <array>
#include <cerrno>
#include <fcntl.h>
#include <iostream>
#include <poll.h>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

namespace {

class LocalPair {
public:
  LocalPair() {
    require(::socketpair(AF_UNIX, SOCK_STREAM, 0, sockets_.data()) == 0, "create local stream");
    for (const int fd : sockets_) {
      const int flags = ::fcntl(fd, F_GETFL, 0);
      if (flags < 0 || ::fcntl(fd, F_SETFL, flags | O_NONBLOCK) != 0) {
        for (const int owned : sockets_)
          ::close(owned);
        throw std::runtime_error("set nonblocking mode");
      }
    }
  }

  LocalPair(const LocalPair&) = delete;
  LocalPair& operator=(const LocalPair&) = delete;

  ~LocalPair() {
    for (const int fd : sockets_)
      ::close(fd);
  }

  int reader() const {
    return sockets_[0];
  }

  int writer() const {
    return sockets_[1];
  }

private:
  std::array<int, 2> sockets_{};
};

void readiness() {
  LocalPair pair;
  pollfd watched{pair.reader(), POLLIN, 0};
  require(::poll(&watched, 1, 0) == 0, "empty stream is not readable");
  std::array<char, 3> bytes{};
  const auto empty = ::recv(pair.reader(), bytes.data(), bytes.size(), 0);
  const bool would_block = empty < 0 && (errno == EAGAIN || errno == EWOULDBLOCK);
  require(would_block, "empty nonblocking recv would block");
  require(::send(pair.writer(), "abc", 3, MSG_NOSIGNAL) == 3, "publish three bytes");
  require(::poll(&watched, 1, 100) == 1 && (watched.revents & POLLIN) != 0,
          "data enables read readiness");
  const auto count = ::recv(pair.reader(), bytes.data(), bytes.size(), 0);
  require(count == 3 && std::string(bytes.data(), bytes.size()) == "abc", "receive available data");
  std::cout << "[A readiness] empty_would_block=" << would_block
            << " readable_after_send=1 bytes=" << count << '\n';
}

void half_close() {
  LocalPair pair;
  require(::send(pair.writer(), "xyz", 3, MSG_NOSIGNAL) == 3, "send before shutdown");
  require(::shutdown(pair.writer(), SHUT_WR) == 0, "half-close sending direction");
  pollfd watched{pair.reader(), POLLIN, 0};
  require(::poll(&watched, 1, 100) == 1, "data or EOF enables readiness");
  std::array<char, 8> bytes{};
  const auto first = ::recv(pair.reader(), bytes.data(), bytes.size(), 0);
  const auto second = ::recv(pair.reader(), bytes.data(), bytes.size(), 0);
  require(first == 3 && second == 0, "queued bytes arrive before EOF");
  require(::send(pair.reader(), "reply", 5, MSG_NOSIGNAL) == 5,
          "the reverse direction remains open");
  const auto reply = ::recv(pair.writer(), bytes.data(), bytes.size(), 0);
  require(reply == 5, "half-closed writer still receives replies");
  std::cout << "[B half-close] first_recv=" << first << " next_recv=" << second
            << " reply_bytes=" << reply << '\n';
}

void backpressure() {
  LocalPair pair;
  const int requested_buffer = 4096;
  require(::setsockopt(pair.writer(), SOL_SOCKET, SO_SNDBUF, &requested_buffer,
                       sizeof(requested_buffer)) == 0,
          "set a small local send buffer");
  const std::string bytes(64 * 1024, 'q');
  const auto first = ::send(pair.writer(), bytes.data(), bytes.size(), MSG_NOSIGNAL);
  require(first > 0 && static_cast<std::size_t>(first) < bytes.size(),
          "a finite local write is partial");
  const auto second = ::send(pair.writer(), "x", 1, MSG_NOSIGNAL);
  const bool would_block = second < 0 && (errno == EAGAIN || errno == EWOULDBLOCK);
  require(would_block, "a peer that does not read fills the send buffer");
  pollfd watched{pair.writer(), POLLOUT, 0};
  require(::poll(&watched, 1, 0) == 0, "full send buffer is not write-ready");
  std::cout << "[C backpressure] requested=" << bytes.size() << " sent=" << first
            << " next_would_block=" << would_block << " writable=0\n";
}

}

int main() {
  try {
    readiness();
    half_close();
    backpressure();
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "[FAIL] " << error.what() << '\n';
    return 1;
  }
}
