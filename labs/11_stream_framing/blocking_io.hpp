#pragma once
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <system_error>
#include <sys/socket.h>
#include <unistd.h>

namespace stream_lab {

// 测试支撑：独占 fd，不实现通信协议或 I/O 算法。
class UniqueFd {
public:
  explicit UniqueFd(int fd = -1) noexcept : fd_(fd) {}

  UniqueFd(const UniqueFd&) = delete;
  UniqueFd& operator=(const UniqueFd&) = delete;

  ~UniqueFd() {
    if (fd_ >= 0)
      ::close(fd_);
  }

  int get() const noexcept {
    return fd_;
  }

private:
  int fd_;
};

// TODO：调用阻塞 send，直到发送完整个 span；空输入直接返回。
// 部分成功只推进实际返回字节数；EINTR 重试，其他错误抛 system_error。
// 正长度 send 返回 0 视为无法继续，抛 runtime_error。
// 本练习在 Linux 上用 MSG_NOSIGNAL 避免断开时 SIGPIPE 终止进程。
// fd 的所有权属于调用者；本函数不 close，不支持 nonblocking fd。
inline void send_all(int fd, std::span<const std::uint8_t> bytes) {
  static_cast<void>(fd);
  static_cast<void>(bytes);
  throw std::logic_error("TODO: stream_lab::send_all");
}

}
