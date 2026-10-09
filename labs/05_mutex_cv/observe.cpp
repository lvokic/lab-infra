#include "lab_check.hpp"
#include <array>
#include <chrono>
#include <condition_variable>
#include <exception>
#include <iostream>
#include <latch>
#include <mutex>
#include <string_view>
#include <thread>

namespace {
// A：lock_guard 保护一次读-改-写；scoped_lock 同时取得两把锁。
void observe_locks() {
  constexpr int iterations = 2000;
  std::mutex counter_mutex;
  int counter = 0;
  const auto increment = [&] {
    for (int i = 0; i < iterations; ++i) {
      std::lock_guard lock(counter_mutex);
      ++counter;
    }
  };
  std::jthread first(increment);
  std::jthread second(increment);
  first.join();
  second.join();
  require(counter == iterations * 2, "locking preserves every increment");
  std::cout << "[A lock_guard] counter=" << counter << " expected=" << iterations * 2 << '\n';

  std::mutex left_mutex;
  std::mutex right_mutex;
  int left = iterations;
  int right = iterations;
  std::jthread to_right([&] {
    for (int i = 0; i < iterations; ++i) {
      std::scoped_lock lock(left_mutex, right_mutex);
      --left;
      ++right;
    }
  });
  std::jthread to_left([&] {
    for (int i = 0; i < iterations; ++i) {
      std::scoped_lock lock(right_mutex, left_mutex);
      --right;
      ++left;
    }
  });
  to_right.join();
  to_left.join();
  require(left == iterations && right == iterations, "two-lock transfers preserve both balances");
  std::cout << "[A scoped_lock] left=" << left << " right=" << right << " total=" << left + right
            << '\n';
}

// B：用 latch 协调观察顺序；真正的数据仍由 mutex/CV 协议保护。
void observe_handshake(bool writer_first) {
  std::mutex mutex;
  std::condition_variable condition;
  std::latch first_false_check(1);
  bool ready = false;
  int payload = 0;
  int observed = 0;
  int predicate_checks = 0;
  bool lock_on_return = false;
  const auto publish = [&] {
    {
      std::lock_guard lock(mutex);
      payload = 42;
      ready = true;
    }
    condition.notify_one();
  };
  if (writer_first)
    publish();  // 此时还没有 reader，通知本身不会被保存。

  std::jthread reader([&] {
    std::unique_lock lock(mutex);
    condition.wait(lock, [&] {
      ++predicate_checks;
      if (predicate_checks == 1 && !ready)
        first_false_check.count_down();
      return ready;
    });
    observed = payload;
    lock_on_return = lock.owns_lock();
  });
  if (!writer_first) {
    first_false_check.wait();
    // 取得 mutex 时，reader 已通过 wait 释放锁，writer 才能更新状态。
    publish();
  }
  reader.join();  // reader 的结果在 join 返回后可由主线程读取。
  require(observed == 42 && lock_on_return, "predicate wait returns with the payload and lock");
  require(writer_first ? predicate_checks == 1 : predicate_checks >= 2,
          "writer-first skips blocking; reader-first checks false before publication");
  std::cout << "[B " << (writer_first ? "writer-first" : "reader-first")
            << "] observed=" << observed << " predicate_checks=" << predicate_checks
            << " lock_on_return=" << lock_on_return << '\n';
}

void observe_ordering() {
  observe_handshake(false);
  observe_handshake(true);
}

// C：让 reader 在一次额外通知之后，明确再次观察到 ready=false。
void observe_notifications() {
  std::mutex mutex;
  std::condition_variable condition;
  std::latch first_check(1);
  std::latch false_rechecked(1);
  bool ready = false;
  bool probe_requested = false;
  int payload = 0;
  int observed = 0;
  int predicate_checks = 0;
  std::jthread reader([&] {
    std::unique_lock lock(mutex);
    condition.wait(lock, [&] {
      ++predicate_checks;
      if (predicate_checks == 1)
        first_check.count_down();
      if (probe_requested && !ready) {
        probe_requested = false;
        false_rechecked.count_down();
      }
      return ready;
    });
    observed = payload;
  });

  first_check.wait();
  {
    std::lock_guard lock(mutex);
    probe_requested = true;
    condition.notify_one();  // 故意在持锁时通知；ready 仍然为 false。
  }
  false_rechecked.wait();
  bool ready_before_publish = false;
  bool reader_still_waiting = false;
  {
    std::lock_guard lock(mutex);
    ready_before_publish = ready;
    reader_still_waiting = observed == 0;
    payload = 42;
    ready = true;
  }
  condition.notify_one();
  reader.join();
  require(!ready_before_publish && reader_still_waiting && observed == 42,
          "notification with a false predicate does not let the reader consume");
  std::cout << "[C extra-notify] ready_before_publish=" << ready_before_publish
            << " predicate_checks=" << predicate_checks << " observed=" << observed << '\n';
}

// D：两个消费者、一份资源；closed 只用于让本次观察中的所有线程退出。
void observe_consumers() {
  std::mutex mutex;
  std::condition_variable condition;
  std::latch first_checks(2);
  std::latch consumed_one(1);
  int available = 0;
  bool closed = false;
  std::array<int, 2> consumed{};
  const auto consume = [&](std::size_t id) {
    bool announced = false;
    std::unique_lock lock(mutex);
    while (true) {
      condition.wait(lock, [&] {
        if (!announced) {
          announced = true;
          first_checks.count_down();
        }
        return available > 0 || closed;
      });
      if (available == 0)
        return;  // 谓词已成立，因此这里意味着 closed 且资源已经耗尽。
      --available;
      ++consumed[id];
      consumed_one.count_down();
    }
  };
  std::jthread second(consume, 1);
  std::jthread first(consume, 0);
  first_checks.wait();
  {
    std::lock_guard lock(mutex);
    available = 1;
  }
  condition.notify_all();
  consumed_one.wait();
  {
    std::lock_guard lock(mutex);
    closed = true;
  }
  condition.notify_all();
  first.join();
  second.join();
  require(consumed[0] + consumed[1] == 1 && available == 0 && closed,
          "one item is consumed exactly once and both consumers exit");
  std::cout << "[D two-consumers] consumed=[" << consumed[0] << ',' << consumed[1]
            << "] remaining=" << available << " closed=" << closed << '\n';
}

// E：没有 writer；谓词始终为 false，固定 deadline 到期后正常返回。
void observe_timeout() {
  using Clock = std::chrono::steady_clock;
  constexpr auto budget = std::chrono::milliseconds(30);
  std::mutex mutex;
  std::condition_variable condition;
  bool ready = false;
  int predicate_checks = 0;
  std::unique_lock lock(mutex);
  const auto started = Clock::now();
  const auto deadline = started + budget;
  const bool satisfied = condition.wait_until(lock, deadline, [&] {
    ++predicate_checks;
    return ready;
  });
  const auto elapsed =
      std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - started);
  require(!satisfied && lock.owns_lock(),
          "timeout reports a false predicate and reacquires the lock");
  std::cout << "[E deadline] satisfied=" << satisfied << " lock_on_return=" << lock.owns_lock()
            << " deadline_ms=" << budget.count() << " elapsed_ms=" << elapsed.count()
            << " predicate_checks=" << predicate_checks << '\n';
}

struct Experiment {
  std::string_view option;
  void (*run)();
};

constexpr std::array experiments{
    Experiment{"--locks", observe_locks},
    Experiment{"--ordering", observe_ordering},
    Experiment{"--notifications", observe_notifications},
    Experiment{"--consumers", observe_consumers},
    Experiment{"--timeout", observe_timeout},
};

void usage() {
  std::cout
      << "Usage: cv_handshake [--all|--locks|--ordering|--notifications|--consumers|--timeout]\n";
}
}

int main(int argc, char* argv[]) {
  if (argc > 2) {
    usage();
    return 2;
  }
  const std::string_view selected = argc == 2 ? argv[1] : "--all";
  if (selected == "--help") {
    usage();
    return 0;
  }
  bool found = false;
  for (const auto& experiment : experiments) {
    if (selected != "--all" && selected != experiment.option)
      continue;
    found = true;
    std::cout << "[RUN] " << experiment.option << std::endl;
    try {
      experiment.run();
    } catch (const std::exception& error) {
      std::cerr << "[FAIL] " << experiment.option << ": " << error.what() << '\n';
      return 1;
    }
    std::cout << "[PASS] " << experiment.option << '\n';
  }
  if (!found) {
    usage();
    return 2;
  }
}
