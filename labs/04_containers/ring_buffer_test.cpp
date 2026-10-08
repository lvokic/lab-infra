#include "check_runner.hpp"
#include "exercise_test_helpers.hpp"
#include "lifetime_probe.hpp"
#include "ring_buffer.hpp"
#include <deque>
#include <random>

int main(int argc, char** argv) {
  ContainerChecks checks(argc, argv);
  checks.run("empty", [] {
    RingBuffer<int> values(3);
    require(values.empty() && !values.full() && values.size() == 0 && values.capacity() == 3,
            "initial buffer is empty with fixed capacity");
    bool rejected = false;
    try {
      RingBuffer<int> invalid(0);
    } catch (const std::invalid_argument&) {
      rejected = true;
    }
    require(rejected, "zero capacity rejected");
  });
  checks.run("fifo", [] {
    RingBuffer<int> values(3);
    const int first = 10;
    require(values.try_push(first) && values.try_push(20) && values.try_push(30), "fill buffer");
    const auto& constant = values;
    require(values.full() && constant.front() == 10 && constant.back() == 30 && constant[1] == 20,
            "logical indexing follows FIFO order");
    values[1] = 21;
    for (int expected : {10, 21, 30}) {
      require(values.front() == expected, "FIFO removal order");
      values.pop();
    }
    require(values.empty(), "drained buffer is empty");
  });
  checks.run("wrap", [] {
    for (std::size_t capacity : {std::size_t{1}, std::size_t{3}, std::size_t{5}}) {
      RingBuffer<int> actual(capacity);
      std::deque<int> expected;
      for (int value = 0; value < 100; ++value) {
        if (expected.size() == capacity) {
          actual.pop();
          expected.pop_front();
        }
        require(actual.try_push(value), "push after freeing a slot");
        expected.push_back(value);
        require_indexed_equal(actual, expected);
      }
    }
  });
  checks.run("full_reject", [] {
    RingBuffer<std::unique_ptr<int>> values(1);
    auto first = std::make_unique<int>(10);
    require(values.try_push(std::move(first)) && !first, "accepted push transfers ownership");
    auto rejected = std::make_unique<int>(20);
    require(!values.try_push(std::move(rejected)) && rejected && *rejected == 20 &&
                values.size() == 1 && *values.front() == 10,
            "full buffer rejects before moving the argument");
    values.pop();
    require(values.try_push(std::move(rejected)) && !rejected && *values.front() == 20,
            "freed slot can be reused");
  });
  checks.run("errors", [] {
    RingBuffer<int> values(2);
    const auto& constant = values;
    require_out_of_range(
        [&] {
          values.pop();
        },
        "empty pop rejected");
    require_out_of_range(
        [&] {
          static_cast<void>(values.front());
        },
        "empty front rejected");
    require_out_of_range(
        [&] {
          static_cast<void>(constant.back());
        },
        "empty const back rejected");
  });
  checks.run("lifetimes", [] {
    LifetimeCounts counts;
    {
      RingBuffer<LifetimeProbe> values(3);
      require(counts.alive == 0, "raw storage does not construct values");
      LifetimeProbe seed(counts, 7);
      require(values.try_push(seed) && values.try_push(LifetimeProbe(counts, 8)),
              "construct values");
      require(counts.alive == 3, "only seed and two values are alive");
      auto* kept = std::addressof(values[1]);
      values.pop();
      require(counts.alive == 2 && std::addressof(values.front()) == kept,
              "pop destroys only front");
      require(values.try_push(seed), "reuse a vacant physical slot");
      values.clear();
      values.clear();
      require(counts.alive == 1 && values.empty() && values.capacity() == 3,
              "clear retains storage");
      require(values.try_push(seed), "reuse after clear");
    }
    require(counts.alive == 0 && counts.constructed == counts.destroyed, "all values destroyed");
  });
  checks.run("exceptions", [] {
    LifetimeCounts counts;
    {
      RingBuffer<LifetimeProbe> values(2);
      LifetimeProbe seed(counts, 7);
      require(values.try_push(seed), "prepare existing value");
      auto* kept = std::addressof(values.front());
      counts.copies_left = 0;
      bool threw = false;
      try {
        values.try_push(seed);
      } catch (const ProbeCopyFailure&) {
        threw = true;
      }
      require(threw && values.size() == 1 && counts.alive == 2 &&
                  std::addressof(values.front()) == kept && values.front().value == 7,
              "failed construction leaves state unchanged");
      counts.copies_left = -1;
      require(values.try_push(seed) && values.full(), "buffer remains usable after failure");
    }
    require(counts.alive == 0 && counts.constructed == counts.destroyed,
            "exception path is balanced");
  });
  checks.run("random", [] {
    RingBuffer<int> actual(7);
    std::deque<int> expected;
    std::mt19937 random(408);
    std::uniform_int_distribution<int> operation(0, 5);
    for (int step = 0; step < 500; ++step) {
      const int op = operation(random);
      if (op < 3) {
        const bool accepted = expected.size() < 7;
        require(actual.try_push(step) == accepted, "capacity admission matches");
        if (accepted)
          expected.push_back(step);
      } else if (op < 5 && !expected.empty()) {
        actual.pop();
        expected.pop_front();
      } else if (op == 5) {
        actual.clear();
        expected.clear();
      }
      require_indexed_equal(actual, expected);
      require(actual.full() == (expected.size() == 7), "full flag matches");
    }
  });
  return checks.finish();
}
