#include "lab_check.hpp"
#include <iostream>
#include <vector>

struct Tracked {
  static inline int alive = 0;
  static inline int copies = 0;
  static inline int moves = 0;
  int value = 0;

  Tracked() { ++alive; }
  explicit Tracked(int input) : value(input) { ++alive; }
  Tracked(const Tracked& other) : value(other.value) { ++alive; ++copies; }
  Tracked(Tracked&& other) noexcept : value(other.value) { ++alive; ++moves; }
  ~Tracked() { --alive; }
};

void show(const std::vector<Tracked>& values) {
  std::cout << "size=" << values.size() << " capacity=" << values.capacity()
            << " data=" << static_cast<const void*>(values.data())
            << " alive=" << Tracked::alive << " copies=" << Tracked::copies
            << " moves=" << Tracked::moves << '\n';
}

int main() {
  {
    std::vector<Tracked> values;
    values.reserve(2);
    require(values.empty() && Tracked::alive == 0, "reserve does not construct elements");
    values.emplace_back(10);
    values.emplace_back(20);
    show(values);
    values.reserve(values.capacity() + 1);
    require(values.size() == 2 && Tracked::alive == 2, "reallocation preserves live elements");
    show(values);
    values.resize(4);
    require(values.size() == 4 && Tracked::alive == 4, "resize constructs elements");
    show(values);
    const auto capacity = values.capacity();
    values.clear();
    require(values.empty() && Tracked::alive == 0 && values.capacity() == capacity,
            "clear destroys elements without releasing capacity");
    show(values);
  }
  require(Tracked::alive == 0, "All elements destroyed");
  // Change noexcept, add a copy-only type, and explain the observed relocation choices.
}
