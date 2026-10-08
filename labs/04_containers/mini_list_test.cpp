#include "check_runner.hpp"
#include "exercise_test_helpers.hpp"
#include "lifetime_probe.hpp"
#include "mini_list.hpp"
#include <list>
#include <random>
#include <vector>

int main(int argc, char** argv) {
  static_assert(std::bidirectional_iterator<MiniList<int>::iterator>);
  static_assert(std::bidirectional_iterator<MiniList<int>::const_iterator>);
  static_assert(!std::is_copy_constructible_v<MiniList<int>>);
  ContainerChecks checks(argc, argv);
  checks.run("empty", [] {
    MiniList<int> values;
    require(values.empty() && values.size() == 0, "initial list is empty");
  });
  checks.run("ends", [] {
    MiniList<int> values;
    const int middle = 20;
    values.push_back(middle);
    values.push_front(10);
    values.push_back(30);
    const auto& constant = values;
    require(values.size() == 3 && constant.front() == 10 && constant.back() == 30,
            "append and prepend preserve both ends");
    values.front() = 11;
    values.back() = 31;
    values.pop_front();
    values.pop_back();
    require(values.size() == 1 && values.front() == 20, "remove both ends");
    values.pop_back();
    require(values.empty() && values.begin() == values.end(), "last removal restores empty list");
  });
  checks.run("iterators", [] {
    MiniList<int> values;
    for (int key : {10, 20, 30})
      values.push_back(key);
    auto it = values.begin();
    require(*it++ == 10 && *it == 20 && *++it == 30, "prefix and postfix increment");
    require(*it-- == 30 && *it == 20 && *--it == 10, "prefix and postfix decrement");
    auto last = values.end();
    require(*--last == 30, "decrement nonempty end");
    MiniList<int>::const_iterator converted = values.begin();
    const auto& constant = values;
    require(converted == constant.begin() && converted == values.begin(), "const conversion");
    require(std::vector<int>(constant.begin(), constant.end()) == std::vector<int>{10, 20, 30},
            "bidirectional iterators work with a standard range constructor");
  });
  checks.run("insert_erase", [] {
    MiniList<int> values;
    values.push_back(10);
    values.push_back(30);
    auto kept = values.begin();
    auto* address = std::addressof(*kept);
    auto position = values.begin();
    ++position;
    const int middle = 20;
    auto inserted = values.insert(position, middle);
    require(*inserted == 20 && *kept == 10 && std::addressof(*kept) == address,
            "middle insertion preserves existing iterators and addresses");
    auto next = values.erase(inserted);
    require(next == position && *next == 30 && values.size() == 2 && *kept == 10,
            "erase returns next and preserves other iterators");
    auto tail = values.insert(values.end(), 40);
    require(*tail == 40 && values.erase(tail) == values.end(), "tail insertion and removal");
    require(values.erase(kept) == values.begin() && values.front() == 30,
            "erasing first node updates begin");
  });
  checks.run("errors", [] {
    MiniList<int> values;
    const auto& constant = values;
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
          values.erase(values.end());
        },
        "erase end rejected");
  });
  checks.run("lifetimes", [] {
    LifetimeCounts counts;
    {
      MiniList<LifetimeProbe> values;
      require(counts.alive == 0, "sentinel does not construct a T");
      LifetimeProbe seed(counts, 7);
      values.push_back(seed);
      values.push_front(LifetimeProbe(counts, 8));
      require(counts.alive == 3 && values.front().value == 8 && values.back().value == 7,
              "only seed and live elements remain");
      auto first = values.begin();
      require(first->value == 8, "iterator arrow accesses the value");
      values.pop_back();
      require(counts.alive == 2, "pop destroys exactly one value");
      values.clear();
      values.clear();
      require(counts.alive == 1 && values.empty() && values.begin() == values.end(),
              "clear destroys values and restores sentinel links");
      values.push_back(seed);
    }
    require(counts.alive == 0 && counts.constructed == counts.destroyed, "destruction balances");
  });
  checks.run("exceptions", [] {
    LifetimeCounts counts;
    {
      MiniList<LifetimeProbe> values;
      LifetimeProbe seed(counts, 7);
      values.push_back(seed);
      const auto old = values.begin();
      counts.copies_left = 0;
      bool threw = false;
      try {
        values.insert(values.begin(), seed);
      } catch (const ProbeCopyFailure&) {
        threw = true;
      }
      require(threw && values.size() == 1 && counts.alive == 2 && values.begin() == old &&
                  old->value == 7,
              "failed insertion leaves links, size and values intact");
      counts.copies_left = -1;
    }
    require(counts.alive == 0 && counts.constructed == counts.destroyed,
            "failed node is cleaned up");
  });
  checks.run("random", [] {
    MiniList<int> actual;
    std::list<int> expected;
    std::mt19937 random(406);
    std::uniform_int_distribution<int> operation(0, 6);
    for (int step = 0; step < 400; ++step) {
      switch (operation(random)) {
        case 0:
          actual.push_front(step);
          expected.push_front(step);
          break;
        case 1:
          actual.push_back(step);
          expected.push_back(step);
          break;
        case 2:
          if (!expected.empty()) {
            actual.pop_front();
            expected.pop_front();
          }
          break;
        case 3:
          if (!expected.empty()) {
            actual.pop_back();
            expected.pop_back();
          }
          break;
        case 4:
        case 5: {
          const auto offset =
              std::uniform_int_distribution<std::size_t>(0, expected.size())(random);
          auto position = actual.begin();
          auto reference = expected.begin();
          std::advance(position, static_cast<std::ptrdiff_t>(offset));
          std::advance(reference, static_cast<std::ptrdiff_t>(offset));
          if (offset < expected.size() && step % 2 == 0) {
            auto next = actual.erase(position);
            auto reference_next = expected.erase(reference);
            require((next == actual.end()) == (reference_next == expected.end()),
                    "erase end result");
            if (reference_next != expected.end())
              require(*next == *reference_next, "erase next value");
          } else {
            require(*actual.insert(position, step) == step, "insert returns new value");
            expected.insert(reference, step);
          }
          break;
        }
        default:
          actual.clear();
          expected.clear();
      }
      const auto& constant = actual;
      require(actual.size() == expected.size() &&
                  std::vector<int>(constant.begin(), constant.end()) ==
                      std::vector<int>(expected.begin(), expected.end()),
              "list matches reference");
    }
  });
  return checks.finish();
}
