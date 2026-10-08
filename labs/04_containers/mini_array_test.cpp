#include "check_runner.hpp"
#include "exercise_test_helpers.hpp"
#include "mini_array.hpp"
#include <algorithm>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace {
struct ArrayProbe {
  inline static int alive = 0;

  ArrayProbe() {
    ++alive;
  }

  ~ArrayProbe() {
    --alive;
  }
};
}  // namespace

int main(int argc, char** argv) {
  static_assert(std::is_same_v<decltype(std::declval<const MiniArray<int, 3>&>()[0]), const int&>);
  ContainerChecks checks(argc, argv);
  checks.run("empty", [] {
    MiniArray<int, 3> values;
    MiniArray<int, 0> zero;
    require(values.size() == 3 && !values.empty() && zero.size() == 0 && zero.empty(),
            "array size is its compile-time extent");
  });
  checks.run("access", [] {
    MiniArray<int, 3> values;
    require(values[0] == 0 && values[1] == 0 && values[2] == 0,
            "exercise value-initializes elements");
    values[0] = 10;
    values.at(1) = 20;
    values.back() = 30;
    const auto& constant = values;
    require(constant.front() == 10 && constant.at(1) == 20 && constant.back() == 30 &&
                std::addressof(values[1]) == values.data() + 1 && constant.data() == values.data(),
            "const access and contiguous storage");
  });
  checks.run("iterators", [] {
    MiniArray<int, 4> values;
    int number = 4;
    for (auto& value : values)
      value = number--;
    require(values.end() - values.begin() == 4 && values.begin() == values.data(), "pointer range");
    std::sort(values.begin(), values.end());
    const auto& constant = values;
    require(std::vector<int>(constant.begin(), constant.end()) == std::vector<int>{1, 2, 3, 4},
            "iterators work with standard algorithms");
  });
  checks.run("fill", [] {
    MiniArray<std::string, 3> values;
    values.fill("hello");
    for (const auto& value : values)
      require(value == "hello", "fill assigns every element");
    values.front() = "changed";
    require(values[1] == "hello", "each element has independent storage");
  });
  checks.run("bounds", [] {
    MiniArray<int, 3> values;
    const auto& constant = values;
    require_out_of_range(
        [&] {
          static_cast<void>(values.at(3));
        },
        "at(size) rejected");
    require_out_of_range(
        [&] {
          static_cast<void>(constant.at(100));
        },
        "const at out of range rejected");
  });
  checks.run("zero", [] {
    MiniArray<int, 0> values;
    const auto& constant = values;
    require(values.data() == nullptr && values.begin() == values.end() &&
                constant.data() == nullptr && constant.begin() == constant.end(),
            "zero-length range");
    values.fill(7);
    require_out_of_range(
        [&] {
          static_cast<void>(values.at(0));
        },
        "zero-length at rejected");
    require_out_of_range(
        [&] {
          static_cast<void>(constant.front());
        },
        "zero-length front rejected");
    require_out_of_range(
        [&] {
          static_cast<void>(values.back());
        },
        "zero-length back rejected");
  });
  checks.run("copy_move", [] {
    MiniArray<std::string, 2> original;
    original[0] = "A";
    original[1] = "B";
    auto copied = original;
    copied[0] = "changed";
    require(original[0] == "A" && copied[1] == "B", "default copy owns independent elements");
    MiniArray<std::unique_ptr<int>, 2> owned;
    owned[0] = std::make_unique<int>(10);
    auto moved = std::move(owned);
    require(!owned[0] && moved[0] && *moved[0] == 10 && !moved[1],
            "default move supports move-only T");
  });
  checks.run("lifetimes", [] {
    require(ArrayProbe::alive == 0, "initial probe state");
    {
      MiniArray<ArrayProbe, 0> zero;
      require(ArrayProbe::alive == 0 && zero.data() == nullptr, "N=0 constructs no hidden T");
      MiniArray<ArrayProbe, 3> values;
      require(ArrayProbe::alive == 3 && values.data() != nullptr,
              "N live elements exist immediately");
    }
    require(ArrayProbe::alive == 0, "member destruction destroys all elements");
  });
  return checks.finish();
}
