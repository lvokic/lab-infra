#include "lab_check.hpp"
#include "lab_exercise_checks.hpp"
#include "lockfree_exercises.hpp"
#include "transfer_driver.hpp"
#include <array>
#include <deque>
#include <exception>
#include <limits>
#include <random>
#include <thread>
#include <vector>

namespace {
using lockfree_exercises::AtomicBudget;
using lockfree_exercises::SpscRing;

template <typename Function>
void invalid_argument(Function operation) {
  bool rejected = false;
  try {
    operation();
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  require(rejected, "invalid capacity throws invalid_argument");
}

void parallel_budget(std::uint64_t limit, std::uint64_t amount, std::uint64_t expected) {
  AtomicBudget budget(limit);
  constexpr std::size_t workers = 4;
  std::array<std::uint64_t, workers> accepted{};
  std::array<std::exception_ptr, workers> errors{};
  std::vector<std::jthread> threads;
  for (std::size_t worker = 0; worker < workers; ++worker) {
    threads.emplace_back([&, worker] {
      try {
        for (int attempt = 0; attempt < 1000; ++attempt) {
          if (budget.try_acquire(amount))
            ++accepted[worker];
        }
      } catch (...) {
        errors[worker] = std::current_exception();
      }
    });
  }
  threads.clear();
  std::uint64_t total = 0;
  for (std::size_t worker = 0; worker < workers; ++worker) {
    if (errors[worker])
      std::rethrow_exception(errors[worker]);
    total += accepted[worker] * amount;
  }
  require(total == expected && budget.used() == expected && budget.used() <= budget.limit(),
          "all successful acquisitions count once and never exceed the limit");
  require(!budget.try_acquire(amount), "no complete request fits after parallel acquisition");
}

}  // namespace

int main(int argc, char** argv) {
  LabExerciseChecks checks(argc, argv);
  checks.run("budget_basic", [] {
    AtomicBudget budget(10);
    require(budget.used() == 0 && budget.limit() == 10, "budget starts unused");
    require(budget.try_acquire(0) && budget.used() == 0, "zero request is a no-op success");
    require(budget.try_acquire(4) && budget.used() == 4, "first request succeeds");
    require(!budget.try_acquire(7) && budget.used() == 4, "rejection leaves state unchanged");
    require(budget.try_acquire(6) && budget.used() == 10, "exact remaining amount fits");
    require(!budget.try_acquire(1) && budget.try_acquire(0) && budget.used() == 10,
            "full budget rejects positive requests and accepts zero");
  });
  checks.run("budget_zero", [] {
    AtomicBudget budget(0);
    require(budget.try_acquire(0) && !budget.try_acquire(1) && budget.used() == 0,
            "zero limit never grants a positive request");
  });
  checks.run("budget_overflow", [] {
    constexpr auto maximum = std::numeric_limits<std::uint64_t>::max();
    AtomicBudget budget(maximum);
    require(budget.try_acquire(maximum - 1), "large request fits");
    require(!budget.try_acquire(2) && budget.used() == maximum - 1,
            "overflowing request is rejected without wrapping");
    require(budget.try_acquire(1) && budget.used() == maximum, "last unit fits");
    require(!budget.try_acquire(maximum), "full counter cannot wrap to a small value");
  });
  checks.run("budget_parallel", [] {
    parallel_budget(2000, 1, 2000);
  });
  checks.run("budget_parallel_chunks", [] {
    parallel_budget(101, 3, 99);
  });
  checks.run("spsc_invalid", [] {
    invalid_argument([] {
      SpscRing queue(0);
    });
    invalid_argument([] {
      SpscRing queue(std::numeric_limits<std::size_t>::max());
    });
  });
  checks.run("spsc_empty", [] {
    SpscRing queue(3);
    require(queue.capacity() == 3 && !queue.try_pop() && !queue.try_pop(),
            "empty calls return nullopt without changing usable capacity");
  });
  checks.run("spsc_single", [] {
    SpscRing queue(1);
    require(queue.try_push(17) && !queue.try_push(18), "capacity one accepts exactly one value");
    require(queue.try_pop() == 17 && !queue.try_pop(), "single value is consumed once");
    require(queue.try_push(19) && queue.try_pop() == 19, "consumed slot can be reused");
  });
  checks.run("spsc_full", [] {
    SpscRing queue(3);
    require(queue.try_push(10) && queue.try_push(20) && queue.try_push(30),
            "declared capacity is usable");
    require(!queue.try_push(99) && !queue.try_push(100), "full calls reject without overwriting");
    require(queue.try_pop() == 10 && queue.try_pop() == 20 && queue.try_pop() == 30 &&
                !queue.try_pop(),
            "rejected pushes preserve all existing data");
  });
  checks.run("spsc_fifo", [] {
    SpscRing queue(4);
    require(queue.try_push(1) && queue.try_push(2) && queue.try_pop() == 1, "initial interleaving");
    require(queue.try_push(3) && queue.try_push(4) && queue.try_pop() == 2,
            "append after consuming an earlier value");
    require(queue.try_pop() == 3 && queue.try_pop() == 4 && !queue.try_pop(), "FIFO order");
  });
  checks.run("spsc_wrap", [] {
    SpscRing queue(3);
    for (int round = 0; round < 200; ++round) {
      require(queue.try_push(round) && queue.try_push(-round), "repeated fill across wrap");
      require(queue.try_pop() == round && queue.try_pop() == -round && !queue.try_pop(),
              "repeated drain across wrap");
    }
  });
  checks.run("spsc_values", [] {
    SpscRing queue(3);
    const std::array<int, 3> values{std::numeric_limits<int>::min(), 0,
                                    std::numeric_limits<int>::max()};
    for (int value : values)
      require(queue.try_push(value), "int values do not act as empty sentinels");
    for (int value : values)
      require(queue.try_pop() == value, "all int representations survive transfer");
  });
  checks.run("spsc_random", [] {
    for (std::size_t capacity : {std::size_t{1}, std::size_t{3}, std::size_t{8}}) {
      SpscRing queue(capacity);
      std::deque<int> reference;
      std::mt19937 random(42);
      for (int step = 0; step < 1000; ++step) {
        if ((random() & 1U) != 0) {
          const int value = static_cast<int>(random() % 201U) - 100;
          const bool expected = reference.size() < capacity;
          require(queue.try_push(value) == expected, "push result matches bounded deque");
          if (expected)
            reference.push_back(value);
        } else {
          const auto value = queue.try_pop();
          require(value.has_value() == !reference.empty(), "pop availability matches deque");
          if (value) {
            require(*value == reference.front(), "random FIFO value matches deque");
            reference.pop_front();
          }
        }
      }
      for (int expected : reference)
        require(queue.try_pop() == expected, "remaining random values match");
      require(!queue.try_pop(), "random table drains fully");
    }
  });
  checks.run("spsc_transfer_small", [] {
    for (std::size_t capacity : {std::size_t{1}, std::size_t{2}, std::size_t{3}}) {
      SpscRing queue(capacity);
      (void)lockfree_driver::transfer(queue, 2048);
    }
  });
  checks.run("spsc_transfer_wrap", [] {
    SpscRing queue(7);
    (void)lockfree_driver::transfer(queue, 8192);
  });
  checks.run("spsc_handoff", [] {
    SpscRing queue(2);
    (void)lockfree_driver::transfer(queue, 1024);
    // 前两线程已经 join，之后才能更换生产者/消费者线程。
    (void)lockfree_driver::transfer(queue, 1024);
    require(queue.try_push(-7) && queue.try_pop() == -7, "quiescent reuse after thread handoff");
  });
  return checks.finish();
}
