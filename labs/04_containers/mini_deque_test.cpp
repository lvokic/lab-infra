#include "check_runner.hpp"
#include "exercise_test_helpers.hpp"
#include "lifetime_probe.hpp"
#include "mini_deque.hpp"
#include <deque>
#include <random>

int main(int argc, char** argv) {
  ContainerChecks checks(argc, argv);
  checks.run("empty", [] {
    MiniDeque<int> values;
    require(values.empty() && values.size() == 0, "initial deque is empty");
  });
  checks.run("ends", [] {
    MiniDeque<int> values;
    const int middle = 20;
    values.push_back(middle);
    values.push_front(10);
    values.push_back(30);
    const auto& constant = values;
    require(constant.front() == 10 && constant.back() == 30 && constant.at(1) == 20,
            "both ends and const indexed access");
    values[1] = 21;
    values.at(0) = 11;
    values.pop_front();
    values.pop_back();
    require(values.size() == 1 && values.front() == 21, "pop both ends");
    values.pop_back();
    require(values.empty(), "last element removed");
    values.push_front(7);
    require(values.back() == 7, "empty deque can restart from the front");
  });
  checks.run("boundaries", [] {
    MiniDeque<int, 3> actual;
    std::deque<int> expected;
    for (int key = 0; key < 80; ++key) {
      actual.push_back(key);
      expected.push_back(key);
      actual.push_front(-key - 1);
      expected.push_front(-key - 1);
      require_indexed_equal(actual, expected);
    }
    while (!expected.empty()) {
      actual.pop_front();
      expected.pop_front();
      if (!expected.empty()) {
        actual.pop_back();
        expected.pop_back();
      }
      require_indexed_equal(actual, expected);
    }
    MiniDeque<int, 1> single_slot_blocks;
    for (int key = 0; key < 10; ++key)
      single_slot_blocks.push_front(key);
    require(single_slot_blocks.size() == 10 && single_slot_blocks[9] == 0,
            "block size one is supported");
  });
  checks.run("stability", [] {
    MiniDeque<int, 2> values;
    values.push_back(42);
    auto* kept = std::addressof(values.front());
    for (int step = 0; step < 100; ++step) {
      values.push_front(-step);
      values.push_back(step);
      require(std::addressof(values[static_cast<std::size_t>(step + 1)]) == kept && *kept == 42,
              "directory growth at either end preserves existing element addresses");
    }
    for (int step = 0; step < 100; ++step) {
      values.pop_front();
      values.pop_back();
    }
    require(values.size() == 1 && std::addressof(values.front()) == kept,
            "removing others keeps value");
  });
  checks.run("errors", [] {
    MiniDeque<int> values;
    const auto& constant = values;
    require_out_of_range(
        [&] {
          values.pop_front();
        },
        "empty pop_front rejected");
    require_out_of_range(
        [&] {
          values.pop_back();
        },
        "empty pop_back rejected");
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
    values.push_back(1);
    require_out_of_range(
        [&] {
          static_cast<void>(values.at(1));
        },
        "at(size) rejected");
    require_out_of_range(
        [&] {
          static_cast<void>(constant.at(2));
        },
        "const at out of range rejected");
  });
  checks.run("lifetimes", [] {
    LifetimeCounts counts;
    {
      MiniDeque<LifetimeProbe, 2> values;
      require(counts.alive == 0, "empty blocks construct no T");
      LifetimeProbe seed(counts, 7);
      for (int step = 0; step < 10; ++step) {
        values.push_back(seed);
        values.push_front(LifetimeProbe(counts, step));
      }
      require(counts.alive == 21 && values.size() == 20, "only live logical elements exist");
      values.pop_front();
      values.pop_back();
      require(counts.alive == 19, "pop destroys only the removed values");
      values.clear();
      values.clear();
      require(counts.alive == 1 && values.empty(), "clear destroys all values");
      values.push_front(seed);
    }
    require(counts.alive == 0 && counts.constructed == counts.destroyed,
            "values destroyed on scope exit");
    MiniDeque<std::unique_ptr<int>, 2> owned;
    owned.push_front(std::make_unique<int>(10));
    owned.push_back(std::make_unique<int>(20));
    require(*owned.front() == 10 && *owned.back() == 20, "move-only values supported");
  });
  checks.run("exceptions", [] {
    LifetimeCounts counts;
    {
      MiniDeque<LifetimeProbe, 1> values;
      LifetimeProbe seed(counts, 7);
      values.push_back(seed);
      auto* kept = std::addressof(values.front());
      counts.copies_left = 0;
      for (bool at_front : {false, true}) {
        bool threw = false;
        try {
          if (at_front)
            values.push_front(seed);
          else
            values.push_back(seed);
        } catch (const ProbeCopyFailure&) {
          threw = true;
        }
        require(threw && values.size() == 1 && counts.alive == 2 &&
                    std::addressof(values.front()) == kept && kept->value == 7,
                "failed construction at either end preserves live elements");
      }
      counts.copies_left = -1;
      values.push_front(seed);
      values.push_back(seed);
      require(values.size() == 3, "deque remains usable after failure");
    }
    require(counts.alive == 0 && counts.constructed == counts.destroyed,
            "failure path is balanced");
  });
  checks.run("random", [] {
    MiniDeque<int, 3> actual;
    std::deque<int> expected;
    std::mt19937 random(409);
    std::uniform_int_distribution<int> operation(0, 7);
    for (int step = 0; step < 600; ++step) {
      switch (operation(random)) {
        case 0:
        case 1:
          actual.push_front(step);
          expected.push_front(step);
          break;
        case 2:
        case 3:
          actual.push_back(step);
          expected.push_back(step);
          break;
        case 4:
          if (!expected.empty()) {
            actual.pop_front();
            expected.pop_front();
          }
          break;
        case 5:
          if (!expected.empty()) {
            actual.pop_back();
            expected.pop_back();
          }
          break;
        case 6:
          if (!expected.empty()) {
            const auto index =
                std::uniform_int_distribution<std::size_t>(0, expected.size() - 1)(random);
            actual[index] = -step;
            expected[index] = -step;
          }
          break;
        default:
          actual.clear();
          expected.clear();
      }
      require_indexed_equal(actual, expected);
    }
  });
  return checks.finish();
}
