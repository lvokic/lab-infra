#include "lab_check.hpp"
#include "sharing_exercises.hpp"
#include <array>
#include <atomic>
#include <chrono>
#include <exception>
#include <iostream>

namespace {

void measure(sharing_exercises::Layout layout, const char* label, std::size_t iterations) {
  constexpr std::size_t workers = 2;
  constexpr std::size_t spacing = 32;
  std::array<std::atomic<std::uint64_t>, spacing + 1> slots{};
  auto& first = sharing_exercises::select_counter(slots, 0, layout, spacing);
  auto& second = sharing_exercises::select_counter(slots, 1, layout, spacing);
  const auto started = std::chrono::steady_clock::now();
  const auto total = sharing_exercises::run(slots, workers, iterations, layout, spacing);
  const auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                      std::chrono::steady_clock::now() - started)
                      .count();
  require(total == iterations * workers, "learner counters preserve all increments");
  std::cout << '[' << label << "] first=" << &first << " second=" << &second
            << " spacing_slots=" << spacing << " spacing_bytes=" << spacing * sizeof(slots[0])
            << " iterations_per_worker=" << iterations << " total=" << total
            << " ns_including_start_join=" << ns << '\n';
}

}

// 固定小规模的测量入口；槽位与线程算法仍由你在 header 中实现。
int main() {
  try {
    using sharing_exercises::Layout;
    constexpr std::size_t iterations = 20000;
    measure(Layout::adjacent, "startup-only", 0);
    for (int round = 0; round < 3; ++round) {
      std::cout << "[round] " << round + 1 << '\n';
      if (round % 2 == 0) {
        measure(Layout::shared, "shared", iterations);
        measure(Layout::adjacent, "adjacent", iterations);
        measure(Layout::spaced, "spaced", iterations);
      } else {
        measure(Layout::spaced, "spaced", iterations);
        measure(Layout::adjacent, "adjacent", iterations);
        measure(Layout::shared, "shared", iterations);
      }
    }
    std::cout << "Spacing is an experiment parameter; timing has no pass/fail threshold.\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "[FAIL] " << error.what() << '\n';
    return 1;
  }
}
