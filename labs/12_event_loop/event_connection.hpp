#pragma once
#include <cerrno>
#include <cstddef>
#include <deque>
#include <poll.h>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <sys/socket.h>
#include <vector>

// 只实现 TODO。这里先传输原始字节；README 最后再接入 L11 parser。
// 类由一个事件线程独占，不允许多个线程同时调用。
class OutboundBuffer {
public:
  explicit OutboundBuffer(std::size_t limit) : limit_(limit) {}

  bool empty() const {
    return chunks_.empty();
  }

  // TODO：原子接纳整个 bytes，若超过“尚未发送字节”的上限则返回 false。
  // 空输入返回 true，不添加空队列节点。接纳时复制内容。
  bool try_enqueue(std::string_view bytes) {
    if (bytes.empty())
      return true;
    size_t len = bytes.size();
    if (pending_ + len > limit_)
      return false;
    pending_ += len;
    chunks_.push_back(std::string{bytes});
    return true;
  }

  // TODO：返回首段尚未发送的 view；空队列返回空 view。
  // view 在后续 enqueue/consume 后不保证有效，不能跨一次 dispatch 保存。
  std::string_view front() const {
    if (chunks_.empty())
      return std::string_view{};
    return std::string_view(chunks_.front()).substr(front_offset_);
  }

  void pop_front() {
    if (chunks_.empty())
      throw std::invalid_argument("");
    chunks_.pop_front();
  }

  // TODO：确认已发送 count 字节，可以跨段；count > pending 抛 out_of_range。
  // 参数不合法时状态保持不变；count == 0 合法。
  void consume(std::size_t count) {
    if (count > pending_)
      throw std::out_of_range("");
    std::size_t remaining = count;
    while (remaining > 0) {
      const std::size_t available = chunks_.front().size() - front_offset_;
      if (remaining < available) {
        front_offset_ += remaining;
        remaining = 0;
      } else {
        remaining -= available;
        chunks_.pop_front();
        front_offset_ = 0;
      }
    }
    pending_ -= count;
  }

  // TODO：返回尚未发送的字节总数。
  std::size_t pending_bytes() const {
    return pending_;
  }

private:
  std::size_t limit_;
  std::deque<std::string> chunks_;
  std::size_t front_offset_ = 0;
  std::size_t pending_ = 0;
};

class PollConnection {
public:
  // fd 是借用，调用者负责 RAII close，并在构造前设为 O_NONBLOCK。
  // TODO：input_limit == 0 抛 invalid_argument；输出上限允许为 0。
  PollConnection(int fd, std::size_t input_limit, std::size_t output_limit)
      : fd_(fd), input_limit_(input_limit), output_(output_limit) {
    if (input_limit_ == 0)
      throw std::invalid_argument("");
  }

  PollConnection(const PollConnection&) = delete;
  PollConnection& operator=(const PollConnection&) = delete;

  int fd() const noexcept {
    return fd_;
  }

  // TODO：交给 poll 的 events。读暂停/输入满/EOF 时不订阅 POLLIN；
  // 仅 pending > 0 时订阅 POLLOUT；failed/done 时返回 0。
  short wanted_events() const {
    if (failed_ || done())
      return 0;
    int events = 0;
    if (!paused_ && !read_eof() && received_.size() < input_limit_)
      events |= POLLIN;
    if (output_.pending_bytes() > 0)
      events |= POLLOUT;
    return static_cast<short>(events);
  }

  // TODO：业务背压开关。暂停只禁止读，已有输出仍可以发送。
  void pause_reads(bool pause) {
    paused_ = pause;
  }

  // TODO：出站接纳；failed 后返回 false，其他情况按 output 上限判断。
  // 允许读 EOF 后排入回复；调用者在回收 fd 前完成业务处理。
  bool queue_output(std::string_view bytes) {
    if (failed())
      return false;
    return output_.try_enqueue(bytes);
  }

  // TODO：移走目前收到的字节，腾出输入容量。返回的内容不能丢失或重复。
  std::string take_received() {
    return std::move(received_);
  }

  // TODO：处理本次 poll 的 revents，按 README 的顺序和返回值契约执行。
  // recv/send 重试 EINTR，EAGAIN/EWOULDBLOCK 停止本方向处理而保留状态。
  // recv == 0 才标记 EOF；POLLHUP 不直接丢弃未读数据。
  // 收发错误、POLLERR/POLLNVAL 标记 failed；普通 I/O 错误不向外抛异常。
  void dispatch(short revents) {
    if (revents & POLLERR || revents & POLLNVAL) {
      failed_ = true;
      return;
    } else if (revents & POLLIN || revents & POLLHUP) {
      while (!paused_ && !read_eof() && received_.size() < input_limit_) {
        char buffer[4096];
        const std::size_t room = input_limit_ - received_.size();
        const std::size_t request = room < sizeof(buffer) ? room : sizeof(buffer);
        const auto count = ::recv(fd_, buffer, request, 0);
        if (count > 0) {
          received_.append(buffer, static_cast<std::size_t>(count));
          continue;
        } else if (count == 0) {
          read_eof_ = true;
          break;
        } else if (errno == EINTR) {
          continue;
        } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
          break;
        }
        failed_ = true;
        break;
      }
    } else if (revents & POLLOUT) {
      while (output_.pending_bytes() > 0) {
        std::string_view view = output_.front();
        const auto count = ::send(fd(), view.data(), view.size(), MSG_NOSIGNAL);
        if (count > 0) {
          output_.consume(static_cast<size_t>(count));
          continue;
        } else if (count < 0 && errno == EINTR) {
          continue;
        } else if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
          break;
        }
        failed_ = true;
        break;
      }
    }
  }

  bool read_eof() const noexcept {
    return read_eof_;
  }

  bool failed() const noexcept {
    return failed_;
  }

  // TODO：failed 或（EOF 且输入已取走且输出已发送）时为 true。
  // EOF 是读半关闭；仍有数据要交业务/发送时不能结束。
  bool done() const {
    if (failed() || (read_eof() && received_.empty() && output_.empty()))
      return true;
    return false;
  }

private:
  int fd_;
  std::size_t input_limit_;
  std::string received_;
  OutboundBuffer output_;
  bool paused_ = false;
  bool read_eof_ = false;
  bool failed_ = false;
};

// TODO：为当前有兴趣事件的连接建立临时 pollfd；没有活动项则直接返回 0。
// done 连接跳过；poll 失败时 EINTR 重试，其他错误抛 system_error。
// 对 revents != 0 的对应连接调用 dispatch，返回处理的连接数。
// timeout_ms 为非负数；此函数只处理一轮，不创建工作线程、不自动 close fd。
// poll 被 EINTR 打断后允许重新使用同一相对 timeout，严格总 deadline 属于选做。
inline std::size_t poll_once(std::span<PollConnection*> connections, int timeout_ms) {
  if (connections.empty())
    return 0;
  std::vector<pollfd> watches;
  watches.reserve(connections.size());
  std::vector<PollConnection*> actives;
  for (size_t i = 0; i < connections.size(); ++i) {
    PollConnection* con = connections[i];
    if (con->done())
      continue;
    pollfd watch{con->fd(), con->wanted_events(), 0};
    watches.push_back(std::move(watch));
    actives.push_back(con);
  }
  const auto count = ::poll(watches.data(), watches.size(), timeout_ms);
  if (count == 0)
    return 0;
  else if (count > 0) {
    for (size_t i = 0; i < actives.size(); ++i) {
      PollConnection* con = actives[i];
      if (watches[i].revents != 0)
        con->dispatch(watches[i].revents);
    }
  } else if (count < 0) {
    if (errno == EINTR) {
      return poll_once(connections, timeout_ms);
    } else {
      throw std::system_error();
    }
  }
  return static_cast<std::size_t>(count);
}
