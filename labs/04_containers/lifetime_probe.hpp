#pragma once
#include <stdexcept>

// 仅用于检查，不属于待实现容器；成功构造才计数。
struct LifetimeCounts {
  int alive = 0;
  int constructed = 0;
  int destroyed = 0;
  int copies_left = -1;
};

struct ProbeCopyFailure : std::runtime_error {
  ProbeCopyFailure() : std::runtime_error("injected copy failure") {}
};

struct LifetimeProbe {
  LifetimeCounts* counts;
  int value;

  LifetimeProbe(LifetimeCounts& input, int number) noexcept : counts(&input), value(number) {
    ++counts->alive;
    ++counts->constructed;
  }

  LifetimeProbe(const LifetimeProbe& other) : counts(other.counts), value(other.value) {
    if (counts->copies_left == 0)
      throw ProbeCopyFailure{};
    if (counts->copies_left > 0)
      --counts->copies_left;
    ++counts->alive;
    ++counts->constructed;
  }

  LifetimeProbe(LifetimeProbe&& other) noexcept : counts(other.counts), value(other.value) {
    other.value = -1;
    ++counts->alive;
    ++counts->constructed;
  }

  ~LifetimeProbe() {
    --counts->alive;
    ++counts->destroyed;
  }
};
