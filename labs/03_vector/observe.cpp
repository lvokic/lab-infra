#include "lab_check.hpp"
#include <cstddef>
#include <iostream>
#include <string_view>
#include <vector>

struct Tracked {
  static inline int alive = 0;
  static inline int constructed = 0;
  static inline int destroyed = 0;
  static inline int copies = 0;
  static inline int moves = 0;
  int value = 0;

  Tracked() {
    ++alive;
    ++constructed;
  }

  explicit Tracked(int input) : value(input) {
    ++alive;
    ++constructed;
  }

  Tracked(const Tracked& other) : value(other.value) {
    ++alive;
    ++constructed;
    ++copies;
  }

  Tracked(Tracked&& other) noexcept : value(other.value) {
    ++alive;
    ++constructed;
    ++moves;
  }

  ~Tracked() {
    --alive;
    ++destroyed;
  }
};

void show_counts() {
  std::cout << " alive=" << Tracked::alive << " constructed=" << Tracked::constructed
            << " destroyed=" << Tracked::destroyed << " copies=" << Tracked::copies
            << " moves=" << Tracked::moves;
}

void show(std::string_view stage, const std::vector<Tracked>& values) {
  std::cout << '[' << stage << "] size=" << values.size() << " capacity=" << values.capacity()
            << " data=" << static_cast<const void*>(values.data()) << " values=[";
  std::string_view separator;
  for (const auto& element : values) {
    std::cout << separator << element.value;
    separator = ", ";
  }
  std::cout << ']';
  show_counts();
  std::cout << '\n';
}

struct Counts {
  int alive = Tracked::alive;
  int constructed = Tracked::constructed;
  int destroyed = Tracked::destroyed;
  int copies = Tracked::copies;
  int moves = Tracked::moves;
};

void show_delta(std::string_view stage, const Counts& before) {
  std::cout << '[' << stage << "] delta_alive=" << Tracked::alive - before.alive
            << " delta_constructed=" << Tracked::constructed - before.constructed
            << " delta_destroyed=" << Tracked::destroyed - before.destroyed
            << " delta_copies=" << Tracked::copies - before.copies
            << " delta_moves=" << Tracked::moves - before.moves << '\n';
}

void observe_capacity() {
  // B1: enough reserved capacity; only the appended elements are constructed.
  {
    std::vector<Tracked> values;
    values.reserve(4);
    const auto capacity = values.capacity();
    const auto* storage = values.data();
    const Counts before;
    show("B1 reserved", values);
    for (int value : {10, 20, 30}) {
      values.emplace_back(value);
      show("B1 append within capacity", values);
    }
    show_delta("B1 append delta", before);
    require(values.size() == 3 && values[0].value == 10 && values[1].value == 20 &&
                values[2].value == 30,
            "appending within capacity preserves order and values");
    require(values.capacity() == capacity && values.data() == storage,
            "appending within capacity keeps the same storage");
    require(Tracked::constructed == before.constructed + 3 &&
                Tracked::destroyed == before.destroyed && Tracked::copies == before.copies &&
                Tracked::moves == before.moves,
            "appending within capacity constructs only the three new elements");
  }
  require(Tracked::alive == 0, "B1 releases its elements before B2 starts");

  // B2: fill the actual capacity, then force reallocation with one append.
  {
    std::vector<Tracked> values;
    values.reserve(2);
    const auto capacity = values.capacity();
    while (values.size() < capacity) {
      values.emplace_back(10);
    }
    show("B2 full", values);
    const Counts before;
    values.emplace_back(20);
    show("B2 append beyond capacity", values);
    show_delta("B2 growth delta", before);
    require(values.size() == capacity + 1 && values.capacity() > capacity,
            "appending to a full vector increases capacity");
    for (std::size_t i = 0; i < capacity; ++i) {
      require(values[i].value == 10, "growth preserves every existing value");
    }
    require(values.back().value == 20, "growth appends the requested new value");
    // The address before growth was printed above; do not use an invalidated pointer.
  }
  require(Tracked::alive == 0, "B2 releases its elements before B3 starts");

  // B3: clear destroys objects; subsequent appends reuse the reserved capacity.
  {
    std::vector<Tracked> values;
    values.reserve(3);
    values.emplace_back(10);
    values.emplace_back(20);
    const auto capacity = values.capacity();
    show("B3 before clear", values);
    const Counts before_clear;
    values.clear();
    show("B3 after clear", values);
    show_delta("B3 clear delta", before_clear);
    require(values.empty() && values.capacity() == capacity && Tracked::alive == 0,
            "clear removes elements and retains capacity");
    require(Tracked::destroyed == before_clear.destroyed + 2, "clear destroys both old elements");

    const Counts before_reuse;
    values.emplace_back(30);
    values.emplace_back(40);
    show("B3 reuse capacity", values);
    show_delta("B3 reuse delta", before_reuse);
    require(values.size() == 2 && values[0].value == 30 && values[1].value == 40 &&
                values.capacity() == capacity,
            "new elements reuse capacity retained by clear");
    require(Tracked::constructed == before_reuse.constructed + 2 &&
                Tracked::destroyed == before_reuse.destroyed &&
                Tracked::copies == before_reuse.copies && Tracked::moves == before_reuse.moves,
            "reusing capacity constructs new elements without relocation");
  }
  std::cout << "[B after all scopes]";
  show_counts();
  std::cout << '\n';
  require(Tracked::alive == 0 && Tracked::constructed == Tracked::destroyed,
          "all experiment B objects are destroyed");
}

int main(int argc, char* argv[]) {
  if (argc == 2 && std::string_view(argv[1]) == "--experiment-b") {
    observe_capacity();
    return 0;
  }
  if (argc != 1) {
    std::cerr << "Usage: vector_lifetime [--experiment-b]\n";
    return 1;
  }
  {
    std::vector<Tracked> values;
    show("empty", values);
    values.reserve(2);
    require(values.empty() && Tracked::alive == 0, "reserve does not construct elements");
    require(values.capacity() >= 2 && Tracked::constructed == 0,
            "reserve allocates capacity without constructing elements");
    show("reserve(2)", values);

    values.emplace_back(10);
    show("emplace_back(10)", values);
    values.emplace_back(20);
    require(values[0].value == 10 && values[1].value == 20,
            "emplace constructs elements with the requested values");
    show("emplace_back(20)", values);

    values.reserve(values.capacity() + 1);
    require(values.size() == 2 && Tracked::alive == 2, "reallocation preserves live elements");
    require(values[0].value == 10 && values[1].value == 20,
            "reallocation preserves element values");
    show("reserve(capacity + 1)", values);

    values.resize(4);
    require(values.size() == 4 && Tracked::alive == 4, "resize constructs elements");
    require(values[0].value == 10 && values[1].value == 20 && values[2].value == 0 &&
                values[3].value == 0,
            "resize preserves existing values and default constructs new elements");
    show("resize(4)", values);

    const auto capacity = values.capacity();
    const auto destroyed = Tracked::destroyed;
    values.clear();
    require(values.empty() && Tracked::alive == 0 && values.capacity() == capacity,
            "clear destroys elements without releasing capacity");
    require(Tracked::destroyed == destroyed + 4, "clear destroys all four live elements");
    show("clear", values);
  }
  // The vector is gone; only the static lifecycle counters remain accessible.
  std::cout << "[after scope]";
  show_counts();
  std::cout << '\n';
  require(Tracked::alive == 0, "All elements destroyed");
  require(Tracked::constructed == Tracked::destroyed,
          "Every constructed object was destroyed exactly once");
  // Change noexcept, add a copy-only type, and explain the observed relocation choices.
}
