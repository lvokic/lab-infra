#include "lab_check.hpp"
#include <atomic>
#include <exception>
#include <iostream>
#include <thread>

int main() {
  try {
    std::atomic<int> counter{0};
    constexpr int per_worker = 1000;
    const auto increment = [&] {
      for (int index = 0; index < per_worker; ++index)
        counter.fetch_add(1, std::memory_order_relaxed);
    };
    std::jthread first(increment);
    std::jthread second(increment);
    first.join();
    second.join();
    const int value = counter.load(std::memory_order_relaxed);
    require(value == 2 * per_worker, "atomic increments are not lost");
    std::cout << "[atomic counter] value=" << value << " expected=" << 2 * per_worker
              << " is_lock_free=" << counter.is_lock_free()
              << " is_always_lock_free=" << std::atomic<int>::is_always_lock_free << '\n';
    std::cout << "[scope] counter only; no ordinary payload is published by this observer\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
