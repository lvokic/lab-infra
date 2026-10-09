#pragma once
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <mutex>
#include <optional>
#include <stdexcept>

// 只实现 TODO。先用 std::deque<int> 管元素，集中练习等待与关闭协议。
// owner 必须先 close，再 join 所有使用者，最后销毁对象。
class BoundedQueue {
public:
  // TODO：拒绝 capacity == 0，抛 std::invalid_argument。
  explicit BoundedQueue(std::size_t capacity) : capacity_(capacity) {
    throw std::logic_error("TODO: BoundedQueue::constructor");
  }

  BoundedQueue(const BoundedQueue&) = delete;
  BoundedQueue& operator=(const BoundedQueue&) = delete;

  // TODO：满且开放时等待；关闭后返回 false，不插入 value。
  // 成功插入队尾时返回 true，并让消费者有机会重新检查状态。
  bool push(int value) {
    static_cast<void>(value);
    throw std::logic_error("TODO: BoundedQueue::push");
  }

  // TODO：空且开放时等待；有元素时取出队首。
  // 关闭后仍排空已有元素；关闭且空时返回 std::nullopt。
  std::optional<int> pop() {
    throw std::logic_error("TODO: BoundedQueue::pop");
  }

  // TODO：永久关闭，可重复调用；生产者与消费者等待者都应能退出。
  void close() {
    throw std::logic_error("TODO: BoundedQueue::close");
  }

private:
  const std::size_t capacity_;
  std::mutex mutex_;
  std::condition_variable not_empty_;
  std::condition_variable not_full_;
  std::deque<int> elements_;
  bool closed_ = false;
};
