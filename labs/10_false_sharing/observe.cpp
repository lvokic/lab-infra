#include "lab_check.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <exception>
#include <iostream>
#include <latch>
#include <thread>
#include <unistd.h>

namespace {
// 实验指定的间隔，并非检测到的 cache line 大小。打印地址后再判断覆盖哪些行。
struct alignas(256) SpacedCounter {
  std::atomic<std::uint64_t> value{0};
};

constexpr std::size_t iterations = 20000;

long long observe(const char* name, std::atomic<std::uint64_t>& first,
                  std::atomic<std::uint64_t>& second, std::size_t count) {
  first.store(0, std::memory_order_relaxed);
  second.store(0, std::memory_order_relaxed);
  std::latch start(1);
  const auto started = std::chrono::steady_clock::now();
  const auto work = [&](std::atomic<std::uint64_t>& counter) {
    start.wait();
    for (std::size_t index = 0; index < count; ++index)
      counter.fetch_add(1, std::memory_order_relaxed);
  };
  std::jthread left([&] {
    work(first);
  });
  std::jthread right([&] {
    work(second);
  });
  start.count_down();
  left.join();
  right.join();
  const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
                           std::chrono::steady_clock::now() - started)
                           .count();
  const auto total = first.load() + (&first == &second ? 0 : second.load());
  require(total == count * 2, "joined counters preserve all increments");
  std::cout << '[' << name << "] iterations_per_thread=" << count << " total=" << total
            << " ns_including_start_join=" << elapsed << '\n';
  return elapsed;
}

void layout(const char* name, const void* first, const void* second, long line_bytes) {
  const auto a = reinterpret_cast<std::uintptr_t>(first);
  const auto b = reinterpret_cast<std::uintptr_t>(second);
  std::cout << "[layout " << name << "] first=" << first << " second=" << second
            << " address_gap=" << (a < b ? b - a : a - b);
  if (line_bytes > 0) {
    const auto line = static_cast<std::uintptr_t>(line_bytes);
    std::cout << " reported_line_start_same=" << (a / line == b / line);
  }
  std::cout << '\n';
}
}  // namespace

// 已完成的小规模观察基线；独立练习实现运行时槽位映射，不复制本结构代替 TODO。
int main() {
  try {
    long reported_line = -1;
#if defined(_SC_LEVEL1_DCACHE_LINESIZE)
    reported_line = ::sysconf(_SC_LEVEL1_DCACHE_LINESIZE);
#endif
    std::cout << "[config] threads=2 rounds=3 configured_spacing=" << sizeof(SpacedCounter)
              << " reported_L1_line_bytes=" << reported_line
              << " atomic_is_always_lock_free=" << std::atomic<std::uint64_t>::is_always_lock_free
              << '\n';
    std::atomic<std::uint64_t> shared{0};
    std::array<std::atomic<std::uint64_t>, 2> adjacent{};
    std::array<SpacedCounter, 2> spaced{};
    layout("shared", &shared, &shared, reported_line);
    layout("adjacent", &adjacent[0], &adjacent[1], reported_line);
    layout("spaced", &spaced[0].value, &spaced[1].value, reported_line);
    observe("startup-only", adjacent[0], adjacent[1], 0);
    std::array<long long, 3> shared_times{}, adjacent_times{}, spaced_times{};
    for (std::size_t round = 0; round < 3; ++round) {
      if (round % 2 == 0) {
        shared_times[round] = observe("true-sharing", shared, shared, iterations);
        adjacent_times[round] = observe("adjacent", adjacent[0], adjacent[1], iterations);
        spaced_times[round] = observe("spaced", spaced[0].value, spaced[1].value, iterations);
      } else {
        spaced_times[round] = observe("spaced", spaced[0].value, spaced[1].value, iterations);
        adjacent_times[round] = observe("adjacent", adjacent[0], adjacent[1], iterations);
        shared_times[round] = observe("true-sharing", shared, shared, iterations);
      }
    }
    for (auto* samples : {&shared_times, &adjacent_times, &spaced_times})
      std::sort(samples->begin(), samples->end());
    std::cout << "[median-ns] shared=" << shared_times[1] << " adjacent=" << adjacent_times[1]
              << " spaced=" << spaced_times[1] << '\n';
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
