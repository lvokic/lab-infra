#include "lab_check.hpp"
#include <array>
#include <cerrno>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

class FileDescriptor {
 public:
  explicit FileDescriptor(int descriptor) : descriptor_(descriptor) {}
  FileDescriptor(const FileDescriptor&) = delete;
  FileDescriptor& operator=(const FileDescriptor&) = delete;
  ~FileDescriptor() { if (descriptor_ >= 0) { ::close(descriptor_); } }
  int get() const { return descriptor_; }
 private:
  int descriptor_;
};

void send_all(int descriptor, const char* bytes, std::size_t count) {
  std::size_t offset = 0;
  while (offset < count) {
    const auto sent = ::send(descriptor, bytes + offset, count - offset, 0);
    if (sent < 0 && errno == EINTR) { continue; }
    if (sent <= 0) { throw std::runtime_error("send failed"); }
    offset += static_cast<std::size_t>(sent);
  }
}

int main() {
  int sockets[2];
  require(::socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) == 0, "Create a local stream");
  FileDescriptor writer(sockets[0]);
  FileDescriptor reader(sockets[1]);
  const std::array<char, 9> wire{0, 0, 0, 5, 'Q', 'U', 'A', 'N', 'T'};
  send_all(writer.get(), wire.data(), 2);
  send_all(writer.get(), wire.data() + 2, wire.size() - 2);
  require(::shutdown(writer.get(), SHUT_WR) == 0, "Half-close writer");

  std::string received;
  std::array<char, 3> chunk;
  for (;;) {
    const auto count = ::recv(reader.get(), chunk.data(), chunk.size(), 0);
    if (count < 0 && errno == EINTR) { continue; }
    if (count < 0) { throw std::runtime_error("recv failed"); }
    if (count == 0) { break; }
    std::cout << "recv bytes=" << count << '\n';
    received.append(chunk.data(), static_cast<std::size_t>(count));
  }

  require(received.size() == wire.size(), "Read all bytes before EOF");
  std::uint32_t length = 0;
  for (std::size_t index = 0; index < 4; ++index) {
    length = (length << 8U) | static_cast<unsigned char>(received[index]);
  }
  require(length == 5 && received.substr(4) == "QUANT", "Decode length and payload");
  std::cout << "Decoded payload=" << received.substr(4) << '\n';
  // This observes a local byte stream. Add incremental framing and TCP loopback in the lab.
}
