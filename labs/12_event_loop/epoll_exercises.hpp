#pragma once
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unordered_map>
#include <unistd.h>
#include <utility>
#include <vector>

// Linux 专项练习。只实现 TODO，契约和学习顺序见 EPOLL_EXERCISES.md。
// 所有方法由同一个事件线程调用；本练习不实现线程安全、ONESHOT 或连接协议。
enum class EpollTrigger { level, edge };

struct EpollReady {
  int fd;
  std::uint32_t events;
};

class EpollSet {
public:
  // TODO：epoll_create1(EPOLL_CLOEXEC)；失败抛 system_error。
  EpollSet() {
    epoll_fd_ = ::epoll_create1(EPOLL_CLOEXEC);
    if (epoll_fd_ == -1)
      throw std::system_error();
  }

  // 已完成：只拥有 epoll 实例 fd，关注的 socket 是借用。
  ~EpollSet() noexcept {
    if (epoll_fd_ >= 0)
      ::close(epoll_fd_);
  }

  EpollSet(const EpollSet&) = delete;
  EpollSet& operator=(const EpollSet&) = delete;
  EpollSet(EpollSet&&) = delete;
  EpollSet& operator=(EpollSet&&) = delete;

  int native_handle() const noexcept {
    return epoll_fd_;
  }

  std::size_t size() const noexcept {
    return interests_.size();
  }

  bool contains(int fd) const {
    return interests_.contains(fd);
  }

  // TODO：EPOLL_CTL_ADD。fd < 0、重复 fd、非法事件位/模式抛 invalid_argument。
  // events 只允许 EPOLLIN|EPOLLOUT|EPOLLRDHUP 的任意子集（包括 0）。
  // EPOLLET 由 trigger 决定；epoll_event.data.fd 保存传入 fd。
  // 内核失败抛 system_error；失败后关注集合保持原样。
  void add(int fd, std::uint32_t events, EpollTrigger trigger = EpollTrigger::level) {
    constexpr std::uint32_t allowed_events =
        static_cast<std::uint32_t>(EPOLLIN | EPOLLOUT | EPOLLRDHUP);
    if (fd < 0 || contains(fd) || (events & ~allowed_events) != 0 ||
        (trigger != EpollTrigger::level && trigger != EpollTrigger::edge)) {
      throw std::invalid_argument("invalid epoll add arguments");
    }
    epoll_event event{};
    event.events = events;
    if (trigger == EpollTrigger::edge)
      event.events |= static_cast<std::uint32_t>(EPOLLET);
    event.data.fd = fd;

    const auto [entry, inserted] = interests_.emplace(fd, Interest{events, trigger});
    if (!inserted)
      throw std::invalid_argument("fd is already registered with epoll");
    if (::epoll_ctl(native_handle(), EPOLL_CTL_ADD, fd, &event) == -1) {
      const int error = errno;
      interests_.erase(entry);
      throw std::system_error(error, std::generic_category(), "epoll_ctl ADD");
    }
  }

  // TODO：EPOLL_CTL_MOD。参数校验同 add；未登记 fd 抛 out_of_range。
  // 允许切换 LT/ET；成功后同步更新用户态登记信息，size 不变。
  void modify(int fd, std::uint32_t events, EpollTrigger trigger = EpollTrigger::level) {
    constexpr std::uint32_t allowed_events =
        static_cast<std::uint32_t>(EPOLLIN | EPOLLOUT | EPOLLRDHUP);
    if (fd < 0 || (events & ~allowed_events) != 0 ||
        (trigger != EpollTrigger::level && trigger != EpollTrigger::edge)) {
      throw std::invalid_argument("invalid epoll modify arguments");
    }
    epoll_event event{};
    event.events = events;
    if (trigger == EpollTrigger::edge)
      event.events |= static_cast<std::uint32_t>(EPOLLET);
    event.data.fd = fd;

    if (!contains(fd))
      throw std::out_of_range("fd is not registered with epoll");
    if (::epoll_ctl(native_handle(), EPOLL_CTL_MOD, fd, &event) == -1) {
      const int error = errno;
      throw std::system_error(error, std::generic_category(), "epoll_ctl MOD");
    }
    interests_[fd] = {events, trigger};
  }

  // TODO：EPOLL_CTL_DEL；fd < 0 抛 invalid_argument，未登记抛 out_of_range。
  // 只取消关注，不 close socket；内核失败不删除用户态登记信息。
  void remove(int fd) {
    if (fd < 0)
      throw std::invalid_argument("invalid");
    if (!contains(fd))
      throw std::out_of_range("fd is already registered with epoll");
    epoll_event event{};
    event.events = interests_[fd].events;
    EpollTrigger trigger = interests_[fd].trigger;
    if (trigger == EpollTrigger::edge)
      event.events |= static_cast<std::uint32_t>(EPOLLET);
    event.data.fd = fd;

    if (::epoll_ctl(native_handle(), EPOLL_CTL_DEL, fd, &event) == -1)
      return;
    interests_.erase(fd);
  }

  // TODO：一轮 epoll_wait，将返回的有效条目转换为 EpollReady。
  // timeout_ms >= 0，1 <= max_events <= 64；非法参数抛 invalid_argument。
  // 校验后，空关注集合直接返回空 vector；其他情况下 0 超时返回空 vector。
  // EINTR 重试，允许重新使用相对 timeout；其他错误抛 system_error。
  // 不在这里执行 recv/send，不要求事件顺序；不能返回未填充的缓冲槽位。
  std::vector<EpollReady> wait(int timeout_ms, std::size_t max_events = 16) {
    if (timeout_ms < 0)
      throw std::invalid_argument("");
    if (max_events < 1 || max_events > 64)
      throw std::invalid_argument("");
    if (interests_.empty())
      return std::vector<EpollReady>{};
    epoll_event event[max_events];
    const int count = ::epoll_wait(native_handle(), event, max_events, timeout_ms);
    if (count > 0) {
      std::vector<EpollReady> ready;
      ready.reserve(count);
      for (int i = 0; i < count; ++i)
        ready.push_back({event[i].data.fd, event[i].events});
      return ready;
    } else if (count == 0) {
      return std::vector<EpollReady>{};
    } else {
      if (errno == EINTR) {
        return wait(timeout_ms, max_events);
      }
      throw std::system_error();
    }
  }

private:
  struct Interest {
    std::uint32_t events;
    EpollTrigger trigger;
  };

  int epoll_fd_ = -1;
  std::unordered_map<int, Interest> interests_;
};

enum class EpollReadStop { would_block, eof, limit };

struct EpollReadBatch {
  std::string bytes;
  EpollReadStop stop;
};

// TODO：从借用的非阻塞 socket 读取，直到 EAGAIN、EOF 或 byte_limit。
// byte_limit == 0 抛 invalid_argument；每次 recv 长度不能超过剩余预算。
// 正数追加实际字节，EINTR 重试，0 停在 eof，EAGAIN/EWOULDBLOCK 停在 would_block。
// 恰好用完预算时返回 limit，不额外 recv 来猜测是否还有数据/EOF。
// 其他 recv 错误抛 system_error；致命错误时不承诺交付此前已读的前缀。
// 本函数不 close、不修改 epoll 关注集合；stop==limit 后由调用者安排主动续读。
inline EpollReadBatch epoll_read_available(int fd, std::size_t byte_limit) {
  if (byte_limit == 0)
    throw std::invalid_argument("byte_limit must be positive");

  std::string bytes;
  char buffer[4096];
  while (bytes.size() < byte_limit) {
    const std::size_t remaining = byte_limit - bytes.size();
    const std::size_t request = remaining < sizeof(buffer) ? remaining : sizeof(buffer);
    const auto count = ::recv(fd, buffer, request, 0);
    if (count > 0) {
      bytes.append(buffer, static_cast<std::size_t>(count));
      continue;
    }
    if (count == 0)
      return {std::move(bytes), EpollReadStop::eof};
    if (errno == EINTR)
      continue;
    if (errno == EAGAIN || errno == EWOULDBLOCK)
      return {std::move(bytes), EpollReadStop::would_block};
    throw std::system_error(errno, std::generic_category(), "recv");
  }
  return {std::move(bytes), EpollReadStop::limit};
}

struct EpollWriteProgress {
  std::size_t sent;
  bool would_block;
};

// TODO：发送 bytes，直到完整发送或 EAGAIN；返回实际确认的字节数。
// 空输入直接返回 {0,false}；部分成功只推进实际返回值，EINTR 重试。
// 使用 MSG_NOSIGNAL；其他错误抛 system_error，正长度 send 返回 0 抛 runtime_error。
// bytes/offset 由调用者拥有；本函数不缓存后缀、不 close socket。
inline EpollWriteProgress epoll_write_available(int fd, std::string_view bytes) {
  if (bytes.empty())
    return {0, false};

  std::size_t sent = 0;
  while (sent < bytes.size()) {
    const auto count = ::send(fd, bytes.data() + sent, bytes.size() - sent, MSG_NOSIGNAL);
    if (count > 0) {
      sent += static_cast<std::size_t>(count);
      continue;
    }
    if (count == 0)
      throw std::runtime_error("send returned 0 for non-empty input");
    if (errno == EINTR)
      continue;
    if (errno == EAGAIN || errno == EWOULDBLOCK)
      return {sent, true};
    throw std::system_error(errno, std::generic_category(), "send");
  }
  return {sent, false};
}
