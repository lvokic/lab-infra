#include "lab_check.hpp"
#include <atomic>
#include "lab_exercise_checks.hpp"
#include <array>
#include <barrier>
#include <iostream>
#include <latch>
#include <mutex>
#include <thread>

namespace {

void basics() {
  std::atomic<int> value{3};
  const int initial = value.load();
  value.store(7);
  const int exchanged = value.exchange(11);
  const int added = value.fetch_add(2);
  const int final = value.load();
  require(initial == 3 && exchanged == 7 && added == 11 && final == 13,
          "exchange and fetch_add return the previous value");
  std::cout << "[A basics] initial=" << initial << " exchange_old=" << exchanged
            << " fetch_add_old=" << added << " final=" << final << '\n';
}

void split_update() {
  std::atomic<int> counter{0};
  std::latch both_loaded(2);
  std::array<int, 2> seen{};
  const auto increment = [&](std::size_t worker) {
    seen[worker] = counter.load(std::memory_order_relaxed);
    both_loaded.count_down();
    both_loaded.wait();  // 两个线程都读完，才允许任意一个写入。
    counter.store(seen[worker] + 1, std::memory_order_relaxed);
  };
  std::jthread first(increment, 0);
  std::jthread second(increment, 1);
  first.join();
  second.join();
  const int final = counter.load(std::memory_order_relaxed);
  require(seen[0] == 0 && seen[1] == 0 && final == 1,
          "separate atomic loads and stores can lose an update");
  std::cout << "[B split update] first_read=" << seen[0] << " second_read=" << seen[1]
            << " final=" << final << " intended=2\n";
}

void rmw_counter() {
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
  const int final = counter.load(std::memory_order_relaxed);
  require(final == 2 * per_worker, "atomic read-modify-write increments are not lost");
  std::cout << "[C RMW counter] final=" << final << " expected=" << 2 * per_worker
            << " is_lock_free=" << counter.is_lock_free()
            << " is_always_lock_free=" << std::atomic<int>::is_always_lock_free << '\n';
}

void cas() {
  std::atomic<int> value{10};
  int expected = 7;
  const bool first = value.compare_exchange_strong(expected, 20);
  const int expected_after_failure = expected;
  const int value_after_failure = value.load();
  const bool second = value.compare_exchange_strong(expected, 20);
  require(!first && expected_after_failure == 10 && value_after_failure == 10 && second &&
              expected == 10 && value.load() == 20,
          "failed CAS updates expected; successful CAS updates the atomic value");
  std::cout << "[D CAS] first_success=" << first
            << " expected_after_failure=" << expected_after_failure
            << " value_after_failure=" << value_after_failure << " second_success=" << second
            << " final=" << value.load() << '\n';
}

void mutex_counter() {
  std::mutex mutex;
  int counter = 0;
  constexpr int per_worker = 1000;
  const auto increment = [&] {
    for (int index = 0; index < per_worker; ++index) {
      std::lock_guard lock(mutex);
      ++counter;
    }
  };
  std::jthread first(increment);
  std::jthread second(increment);
  first.join();
  second.join();
  require(counter == 2 * per_worker, "mutex protects the entire ordinary increment");
  std::cout << "[E mutex counter] final=" << counter << " expected=" << 2 * per_worker << '\n';
}

void observe_order(std::memory_order order, const char* label) {
  constexpr int rounds = 128;
  std::atomic<int> x{0};
  std::atomic<int> y{0};
  std::array<int, 2> reads{};
  std::array<int, 4> outcomes{};
  std::barrier phase(3);
  std::jthread first([&] {
    for (int round = 0; round < rounds; ++round) {
      phase.arrive_and_wait();
      x.store(1, order);
      reads[0] = y.load(order);
      phase.arrive_and_wait();
    }
  });
  std::jthread second([&] {
    for (int round = 0; round < rounds; ++round) {
      phase.arrive_and_wait();
      y.store(1, order);
      reads[1] = x.load(order);
      phase.arrive_and_wait();
    }
  });
  for (int round = 0; round < rounds; ++round) {
    x.store(0, std::memory_order_relaxed);
    y.store(0, std::memory_order_relaxed);
    phase.arrive_and_wait();
    phase.arrive_and_wait();
    ++outcomes[static_cast<std::size_t>(reads[0] * 2 + reads[1])];
  }
  first.join();
  second.join();
  if (order == std::memory_order_seq_cst)
    require(outcomes[0] == 0, "seq_cst store/load ordering excludes both reads seeing zero");
  std::cout << "[F ordering " << label << "] rounds=" << rounds << " reads_00=" << outcomes[0]
            << " reads_01=" << outcomes[1] << " reads_10=" << outcomes[2]
            << " reads_11=" << outcomes[3] << '\n';
}

void ordering() {
  observe_order(std::memory_order_relaxed, "relaxed");
  observe_order(std::memory_order_seq_cst, "seq_cst");
  std::cout << "[scope] relaxed 00 is permitted, not required; all shared x/y accesses are atomic; "
               "this does not implement ordinary payload publication\n";
}

}

int main(int argc, char** argv) {
  LabExerciseChecks checks(argc, argv);
  checks.run("basics", basics);
  checks.run("split_update", split_update);
  checks.run("rmw_counter", rmw_counter);
  checks.run("cas", cas);
  checks.run("mutex_counter", mutex_counter);
  checks.run("ordering", ordering);
  return checks.finish();
}
