#pragma once
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <vector>

namespace lockfree_exercises {

// 只增不减的额度计数器，不发布其他数据；允许多个线程同时申请。
class AtomicBudget {
public:
  explicit AtomicBudget(std::uint64_t limit) noexcept : limit_(limit) {}

  AtomicBudget(const AtomicBudget&) = delete;
  AtomicBudget& operator=(const AtomicBudget&) = delete;

  // TODO A：用 CAS 重试申请 amount。成功才改变 used_，且始终 used_ <= limit_。
  // amount==0 成功但不改变状态；额度不足返回 false。检查加法溢出。
  // 不使用 mutex、fetch_add 后回滚或 load + store 的组合。
  bool try_acquire(std::uint64_t amount) {
    (void)amount;
    throw std::logic_error("TODO: AtomicBudget::try_acquire");
  }

  std::uint64_t used() const noexcept {
    return used_.load(std::memory_order_relaxed);
  }

  std::uint64_t limit() const noexcept {
    return limit_;
  }

  bool atomics_are_lock_free() const noexcept {
    return used_.is_lock_free();
  }

private:
  const std::uint64_t limit_;
  std::atomic<std::uint64_t> used_{0};
};

// 固定容量、普通 int 槽位；最多一个生产者和一个消费者。
// 对象须在线程启动前构造，在所有线程退出后销毁。没有并发 reset/close/resize。
class SpscRing {
public:
  // TODO B：capacity 是可存元素数；额外留一个槽位区分满和空。
  // capacity==0 或 SIZE_MAX 抛 invalid_argument；其他分配失败正常传播。
  // 只在构造时分配，以后 try_push/try_pop 不分配。
  explicit SpscRing(std::size_t capacity) {
    (void)capacity;
    throw std::logic_error("TODO: SpscRing::constructor");
  }

  SpscRing(const SpscRing&) = delete;
  SpscRing& operator=(const SpscRing&) = delete;
  SpscRing(SpscRing&&) = delete;
  SpscRing& operator=(SpscRing&&) = delete;

  // TODO C：满则立即返回 false，并保持已有数据。
  // 写槽位与发布写位置的顺序，由你选择内存序并证明。
  bool try_push(int value) {
    (void)value;
    throw std::logic_error("TODO: SpscRing::try_push");
  }

  // TODO D：空则立即返回 nullopt；成功按 FIFO 返回一个值。
  // 读槽位与公布槽位可复用的顺序，也需要一条跨线程同步关系。
  std::optional<int> try_pop() {
    throw std::logic_error("TODO: SpscRing::try_pop");
  }

  std::size_t capacity() const noexcept {
    return slots_.empty() ? 0 : slots_.size() - 1;
  }

  // 只报告原子成员是否无锁，不替整个算法作进展证明。
  bool atomics_are_lock_free() const noexcept {
    return read_position_.is_lock_free() && write_position_.is_lock_free();
  }

private:
  std::vector<int> slots_;
  std::atomic<std::size_t> read_position_{0};
  std::atomic<std::size_t> write_position_{0};
};

}  // namespace lockfree_exercises
