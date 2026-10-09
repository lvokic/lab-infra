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

  // TODO：原子接纳整个 bytes，若超过“尚未发送字节”的上限则返回 false。
  // 空输入返回 true，不添加空队列节点。接纳时复制内容。
  bool try_enqueue(std::string_view bytes) {
    static_cast<void>(bytes);
    throw std::logic_error("TODO: OutboundBuffer::try_enqueue");
  }

  // TODO：返回首段尚未发送的 view；空队列返回空 view。
  // view 在后续 enqueue/consume 后不保证有效，不能跨一次 dispatch 保存。
  std::string_view front() const {
    throw std::logic_error("TODO: OutboundBuffer::front");
  }

  // TODO：确认已发送 count 字节，可以跨段；count > pending 抛 out_of_range。
  // 参数不合法时状态保持不变；count == 0 合法。
  void consume(std::size_t count) {
    static_cast<void>(count);
    throw std::logic_error("TODO: OutboundBuffer::consume");
  }

  // TODO：返回尚未发送的字节总数。
  std::size_t pending_bytes() const {
    throw std::logic_error("TODO: OutboundBuffer::pending_bytes");
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
    throw std::logic_error("TODO: PollConnection::constructor");
  }

  PollConnection(const PollConnection&) = delete;
  PollConnection& operator=(const PollConnection&) = delete;

  int fd() const noexcept {
    return fd_;
  }

  // TODO：交给 poll 的 events。读暂停/输入满/EOF 时不订阅 POLLIN；
  // 仅 pending > 0 时订阅 POLLOUT；failed/done 时返回 0。
  short wanted_events() const {
    throw std::logic_error("TODO: PollConnection::wanted_events");
  }

  // TODO：业务背压开关。暂停只禁止读，已有输出仍可以发送。
  void pause_reads(bool pause) {
    static_cast<void>(pause);
    throw std::logic_error("TODO: PollConnection::pause_reads");
  }

  // TODO：出站接纳；failed 后返回 false，其他情况按 output 上限判断。
  // 允许读 EOF 后排入回复；调用者在回收 fd 前完成业务处理。
  bool queue_output(std::string_view bytes) {
    static_cast<void>(bytes);
    throw std::logic_error("TODO: PollConnection::queue_output");
  }

  // TODO：移走目前收到的字节，腾出输入容量。返回的内容不能丢失或重复。
  std::string take_received() {
    throw std::logic_error("TODO: PollConnection::take_received");
  }

  // TODO：处理本次 poll 的 revents，按 README 的顺序和返回值契约执行。
  // recv/send 重试 EINTR，EAGAIN/EWOULDBLOCK 停止本方向处理而保留状态。
  // recv == 0 才标记 EOF；POLLHUP 不直接丢弃未读数据。
  // 收发错误、POLLERR/POLLNVAL 标记 failed；普通 I/O 错误不向外抛异常。
  void dispatch(short revents) {
    static_cast<void>(revents);
    throw std::logic_error("TODO: PollConnection::dispatch");
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
    throw std::logic_error("TODO: PollConnection::done");
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
  static_cast<void>(connections);
  static_cast<void>(timeout_ms);
  throw std::logic_error("TODO: poll_once");
}
