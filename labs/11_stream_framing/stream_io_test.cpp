#include "blocking_io.hpp"
#include "lab_check.hpp"
#include "lab_exercise_checks.hpp"
#include <array>
#include <netinet/in.h>
#include <poll.h>
#include <string>

namespace {

std::array<int, 2> make_pair() {
  std::array<int, 2> pair{};
  require(::socketpair(AF_UNIX, SOCK_STREAM, 0, pair.data()) == 0, "create local stream pair");
  return pair;
}

// 完整测试支撑：限定每次读取最多三个字节，与学习者的发送实现分开。
std::string receive_to_eof(int fd) {
  std::string result;
  for (;;) {
    pollfd watched{fd, POLLIN, 0};
    int ready;
    do {
      ready = ::poll(&watched, 1, 500);
    } while (ready < 0 && errno == EINTR);
    require(ready > 0, "reader reaches bytes or EOF within the test deadline");
    std::array<char, 3> chunk{};
    const auto count = ::recv(fd, chunk.data(), chunk.size(), MSG_DONTWAIT);
    if (count < 0 && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK))
      continue;
    require(count >= 0, "receive succeeds");
    if (count == 0)
      return result;
    result.append(chunk.data(), static_cast<std::size_t>(count));
    require(result.size() <= 1024, "test transport contains only a small finite payload");
  }
}

void send_and_check(int writer, int reader) {
  const std::array<std::uint8_t, 9> bytes{0, 0, 0, 5, 'Q', 0, 0xff, 'N', 'T'};
  stream_lab::send_all(writer, bytes);
  require(::shutdown(writer, SHUT_WR) == 0, "shutdown writes without discarding queued bytes");
  const auto received = receive_to_eof(reader);
  require(received.size() == bytes.size(), "all bytes arrive before EOF");
  for (std::size_t index = 0; index < bytes.size(); ++index)
    require(static_cast<std::uint8_t>(received[index]) == bytes[index], "binary bytes unchanged");
}

template <typename Function>
void expect_system_error(Function function) {
  bool caught = false;
  try {
    function();
  } catch (const std::system_error&) {
    caught = true;
  }
  require(caught, "send failure is reported as system_error");
}

}

int main(int argc, char** argv) {
  LabExerciseChecks checks(argc, argv);
  checks.run("empty", [] {
    stream_lab::send_all(-1, {});
  });
  checks.run("socketpair", [] {
    const auto pair = make_pair();
    stream_lab::UniqueFd writer(pair[0]);
    stream_lab::UniqueFd reader(pair[1]);
    send_and_check(writer.get(), reader.get());
  });
  checks.run("invalid_fd", [] {
    const std::array<std::uint8_t, 1> bytes{42};
    expect_system_error([&] {
      stream_lab::send_all(-1, bytes);
    });
  });
  checks.run("closed_peer", [] {
    const auto pair = make_pair();
    stream_lab::UniqueFd writer(pair[0]);
    { stream_lab::UniqueFd reader(pair[1]); }
    const std::array<std::uint8_t, 1> bytes{42};
    expect_system_error([&] {
      stream_lab::send_all(writer.get(), bytes);
    });
  });
  checks.run("tcp_loopback", [] {
    stream_lab::UniqueFd listener(::socket(AF_INET, SOCK_STREAM, 0));
    require(listener.get() >= 0, "create TCP listener");
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = 0;
    require(::bind(listener.get(), reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0,
            "bind loopback with an OS-selected port");
    require(::listen(listener.get(), 1) == 0, "listen on loopback");
    socklen_t length = sizeof(address);
    require(::getsockname(listener.get(), reinterpret_cast<sockaddr*>(&address), &length) == 0,
            "obtain assigned port");
    stream_lab::UniqueFd client(::socket(AF_INET, SOCK_STREAM, 0));
    require(client.get() >= 0, "create client");
    require(::connect(client.get(), reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0,
            "connect to the same process on loopback");
    stream_lab::UniqueFd server(::accept(listener.get(), nullptr, nullptr));
    require(server.get() >= 0, "accept client");
    send_and_check(client.get(), server.get());
  });
  return checks.finish();
}
