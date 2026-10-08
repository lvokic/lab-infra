#include "check_runner.hpp"
#include "exercise_test_helpers.hpp"
#include "mini_heap.hpp"
#include <algorithm>
#include <memory>
#include <queue>
#include <random>

int main(int argc, char** argv) {
  ContainerChecks checks(argc, argv);
  checks.run("empty", [] {
    MiniHeap<int> values;
    require(values.empty() && values.size() == 0, "initial heap is empty");
  });
  checks.run("push_pop", [] {
    MiniHeap<int> values;
    const int seed = 4;
    values.push(seed);
    for (int value : {1, 7, 7, -2})
      values.push(value);
    for (int expected : {7, 7, 4, 1, -2}) {
      const auto view = values.debug_values();
      require(std::is_heap(view.begin(), view.end()) && values.top() == expected,
              "maximum and heap invariant hold");
      values.pop();
    }
    require(values.empty(), "all values removed");
  });
  checks.run("min_heap", [] {
    MiniHeap<int, std::greater<int>> values;
    for (int value : {4, -1, 9, 2})
      values.push(value);
    for (int expected : {-1, 2, 4, 9}) {
      const auto view = values.debug_values();
      require(std::is_heap(view.begin(), view.end(), std::greater<int>{}) &&
                  values.top() == expected,
              "custom comparator produces a minimum heap");
      values.pop();
    }
  });
  checks.run("heapify", [] {
    MiniHeap<int> values;
    values.assign({3, 9, 1, 8, 2, 7, 6, 5, 4});
    require(values.size() == 9, "bulk construction keeps all values");
    const auto view = values.debug_values();
    require(std::is_heap(view.begin(), view.end()), "bulk construction establishes heap order");
    for (int expected = 9; expected > 0; --expected) {
      require(values.top() == expected, "bulk heap pops in order");
      values.pop();
    }
    values.assign({});
    require(values.empty(), "assign empty range");
  });
  checks.run("errors", [] {
    MiniHeap<int> values;
    require_out_of_range(
        [&] {
          static_cast<void>(values.top());
        },
        "empty top rejected");
    require_out_of_range(
        [&] {
          values.pop();
        },
        "empty pop rejected");
  });
  checks.run("move_only", [] {
    struct Less {
      bool operator()(const std::unique_ptr<int>& left,
                      const std::unique_ptr<int>& right) const noexcept {
        return *left < *right;
      }
    };
    MiniHeap<std::unique_ptr<int>, Less> values;
    values.push(std::make_unique<int>(2));
    values.push(std::make_unique<int>(5));
    values.push(std::make_unique<int>(1));
    for (int expected : {5, 2, 1}) {
      require(values.top() && *values.top() == expected, "move-only heap preserves ownership");
      values.pop();
    }
  });
  checks.run("reuse", [] {
    MiniHeap<int> values;
    values.push(4);
    values.clear();
    values.clear();
    require(values.empty(), "clear removes all elements");
    values.push(8);
    require(values.size() == 1 && values.top() == 8, "reuse after clear");
  });
  checks.run("random", [] {
    MiniHeap<int> actual;
    std::priority_queue<int> expected;
    std::mt19937 random(407);
    std::uniform_int_distribution<int> operation(0, 4);
    std::uniform_int_distribution<int> numbers(-100, 100);
    for (int step = 0; step < 500; ++step) {
      const int op = operation(random);
      if (op < 3) {
        const int value = numbers(random);
        actual.push(value);
        expected.push(value);
      } else if (op == 3 && !expected.empty()) {
        actual.pop();
        expected.pop();
      } else if (op == 4) {
        actual.clear();
        expected = {};
      }
      require(actual.size() == expected.size(), "heap size matches reference");
      if (!expected.empty())
        require(actual.top() == expected.top(), "heap top matches reference");
      const auto view = actual.debug_values();
      require(std::is_heap(view.begin(), view.end()), "heap order survives random operations");
    }
  });
  return checks.finish();
}
