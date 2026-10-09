#include "lab_check.hpp"
#include "lab_exercise_checks.hpp"
#include "sharing_exercises.hpp"
#include <array>
#include <limits>

int main(int argc, char** argv) {
  LabExerciseChecks checks(argc, argv);
  checks.run("slots", [] {
    using sharing_exercises::Layout;
    std::array<std::atomic<std::uint64_t>, 10> slots{};
    require(&sharing_exercises::select_counter(slots, 1, Layout::shared, 3) == &slots[0],
            "shared maps both workers to one object");
    require(&sharing_exercises::select_counter(slots, 1, Layout::adjacent, 3) == &slots[1],
            "adjacent has one object per worker");
    require(&sharing_exercises::select_counter(slots, 1, Layout::spaced, 3) == &slots[3],
            "spacing counts atomic slots");
    require(&sharing_exercises::select_counter(slots, 3, Layout::spaced, 3) == &slots[9],
            "last valid spaced slot");
  });
  checks.run("invalid", [] {
    using sharing_exercises::Layout;
    std::array<std::atomic<std::uint64_t>, 2> slots{};
    bool spacing = false, bounds = false, overflow = false, workers = false, empty = false;
    try {
      (void)sharing_exercises::select_counter(slots, 0, Layout::spaced, 0);
    } catch (const std::invalid_argument&) {
      spacing = true;
    }
    try {
      (void)sharing_exercises::select_counter(slots, 1, Layout::spaced, 2);
    } catch (const std::out_of_range&) {
      bounds = true;
    }
    try {
      (void)sharing_exercises::select_counter(slots, std::numeric_limits<std::size_t>::max(),
                                              Layout::spaced, 2);
    } catch (const std::out_of_range&) {
      overflow = true;
    }
    try {
      (void)sharing_exercises::run(slots, 3, 10, Layout::shared, 1);
    } catch (const std::invalid_argument&) {
      workers = true;
    }
    try {
      (void)sharing_exercises::run(slots, 0, 10, Layout::shared, 1);
    } catch (const std::invalid_argument&) {
      empty = true;
    }
    require(spacing && bounds && overflow && workers && empty, "invalid parameters rejected");
    bool no_partial_work = false;
    try {
      (void)sharing_exercises::run(slots, 2, 10, Layout::spaced, 2);
    } catch (const std::out_of_range&) {
      no_partial_work = true;
    }
    require(no_partial_work && slots[0].load() == 0 && slots[1].load() == 0,
            "validate layout before launching workers");
  });
  checks.run("increment", [] {
    std::atomic<std::uint64_t> counter{7};
    sharing_exercises::increment(counter, 0);
    require(counter.load() == 7, "zero increments preserves value");
    sharing_exercises::increment(counter, 100);
    require(counter.load() == 107, "increments accumulate");
  });
  checks.run("concurrent", [] {
    using sharing_exercises::Layout;
    for (const auto layout : {Layout::shared, Layout::adjacent, Layout::spaced}) {
      std::array<std::atomic<std::uint64_t>, 5> slots{};
      require(sharing_exercises::run(slots, 2, 1000, layout, 4) == 2000, "no lost increments");
      require(sharing_exercises::run(slots, 2, 10, layout, 4) == 2020, "second run accumulates");
      const std::size_t second =
          layout == Layout::shared ? 0 : (layout == Layout::adjacent ? 1 : 4);
      for (std::size_t index = 0; index < slots.size(); ++index)
        if (index != 0 && index != second)
          require(slots[index].load() == 0, "unused slots unchanged");
      require(sharing_exercises::run(slots, 1, 0, layout, 4) == slots[0].load(),
              "single worker and zero iterations");
    }
  });
  return checks.finish();
}
