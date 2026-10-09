#pragma once
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>

namespace sharing_exercises {
enum class Layout { shared, adjacent, spaced };

// shared 使用 slots[0]；adjacent 使用 slots[worker]；spaced 使用 slots[worker*spacing]。
// spacing 是“槽数”，不是字节数，也不是硬件 cache line 大小。
// spacing==0 抛 invalid_argument；下标乘法溢出或超出 slots 抛 out_of_range。
// 非 spaced 布局也要求 spacing>0，以保持统一参数契约。
inline std::atomic<std::uint64_t>& select_counter(std::span<std::atomic<std::uint64_t>> slots,
                                                  std::size_t worker, Layout layout,
                                                  std::size_t spacing) {
  (void)slots;
  (void)worker;
  (void)layout;
  (void)spacing;
  throw std::logic_error("TODO: select_counter");
}

// 恰好 iterations 次 fetch_add(1, memory_order_relaxed)，不重置原有值。
inline void increment(std::atomic<std::uint64_t>& counter, std::size_t iterations) {
  (void)counter;
  (void)iterations;
  throw std::logic_error("TODO: increment");
}

// 在启动任何线程前校验所有槽位；workers>2 或 workers==0 抛 invalid_argument。
// 每 worker 使用 select_counter 的槽位并调用 increment；全部 join 后返回这些槽位的总和。
// shared 槽位只计入总和一次。不要重置 slots（允许多轮累计），未选中槽不变。
// 测试传入的初值和迭代数保证不发生 uint64_t 溢出。
// 提示：工作线程不抛异常；主线程提前验证布局，局部 jthread 容器管理生命周期。
inline std::uint64_t run(std::span<std::atomic<std::uint64_t>> slots, std::size_t workers,
                         std::size_t iterations, Layout layout, std::size_t spacing) {
  (void)slots;
  (void)workers;
  (void)iterations;
  (void)layout;
  (void)spacing;
  throw std::logic_error("TODO: run");
}
}  // namespace sharing_exercises
