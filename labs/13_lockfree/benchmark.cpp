#include "lab_check.hpp"
#include "lockfree_exercises.hpp"
#include "transfer_driver.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <exception>
#include <iostream>
#include <string_view>

int main(int argc, char** argv) {
  if (argc == 2 && std::string_view(argv[1]) == "--help") {
    std::cout << "Usage: lockfree_benchmark\n"
              << "Three 20000-value SPSC transfers; includes thread creation, join and yielding.\n";
    return 0;
  }
  if (argc != 1) {
    std::cerr << "Usage: lockfree_benchmark [--help]\n";
    return 2;
  }
  try {
    constexpr int count = 20000;
    lockfree_exercises::SpscRing queue(64);
    std::cout << "[configuration] count=" << count << " capacity=" << queue.capacity()
              << " atomic_members_lock_free=" << queue.atomics_are_lock_free() << '\n';
    (void)lockfree_driver::transfer(queue, 1024);
    std::array<long long, 3> samples{};
    for (std::size_t round = 0; round < samples.size(); ++round) {
      const auto started = std::chrono::steady_clock::now();
      const auto checksum = lockfree_driver::transfer(queue, count);
      samples[round] = std::chrono::duration_cast<std::chrono::nanoseconds>(
                           std::chrono::steady_clock::now() - started)
                           .count();
      std::cout << "[transfer] round=" << round << " ns=" << samples[round]
                << " checksum=" << checksum << '\n';
    }
    std::sort(samples.begin(), samples.end());
    std::cout << "[median] ns=" << samples[1] << " transferred=" << count << '\n';
    std::cout << "This is whole-transfer timing, not per-message latency or a lock-free proof.\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
