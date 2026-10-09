#include "lab_check.hpp"
#include "lab_exercise_checks.hpp"
#include "locality_exercises.hpp"
#include <array>
#include <limits>
#include <numeric>
#include <random>
#include <algorithm>
#include <vector>

int main(int argc, char** argv) {
  LabExerciseChecks checks(argc, argv);
  checks.run("ordered", [] {
    const std::array<std::uint32_t, 4> values{10, 20, 30, 40};
    const std::array<std::size_t, 4> forward{0, 1, 2, 3}, shuffled{2, 0, 3, 1};
    const std::array<std::size_t, 3> repeated{1, 1, 3};
    require(locality_exercises::sum_ordered(values, forward) == 100, "sequential checksum");
    require(locality_exercises::sum_ordered(values, shuffled) == 100, "permutation checksum");
    require(locality_exercises::sum_ordered(values, repeated) == 80, "repeated visits count");
    require(locality_exercises::sum_ordered(values, {}) == 0, "empty order");
    std::vector<std::uint32_t> many(257);
    std::iota(many.begin(), many.end(), 0U);
    std::vector<std::size_t> order(many.size());
    std::iota(order.begin(), order.end(), std::size_t{0});
    std::mt19937 random(42);
    std::shuffle(order.begin(), order.end(), random);
    require(locality_exercises::sum_ordered(many, order) == 32896, "fixed-seed permutation");
  });
  checks.run("stride", [] {
    const std::array<std::uint32_t, 5> values{1, 2, 3, 4, 5};
    require(locality_exercises::sum_stride(values, 1) == 15, "unit stride");
    require(locality_exercises::sum_stride(values, 2) == 9, "last valid element");
    require(locality_exercises::sum_stride(values, 3) == 5, "partial final interval");
    require(locality_exercises::sum_stride(values, 99) == 1, "stride larger than region");
    require(locality_exercises::sum_stride(values, std::numeric_limits<std::size_t>::max()) == 1,
            "very large stride does not wrap");
    require(locality_exercises::sum_stride({}, 2) == 0, "empty values");
  });
  checks.run("chase", [] {
    const std::array<std::size_t, 4> next{2, 3, 1, 0};
    require(locality_exercises::chase(next, 0, 4) == 6, "cycle visits every node");
    require(locality_exercises::chase(next, 0, 8) == 12, "repeat cycle");
    require(locality_exercises::chase(next, 2, 3) == 6, "start and step count matter");
    require(locality_exercises::chase({}, 99, 0) == 0, "zero steps performs no reads");
    const std::array<std::size_t, 1> terminal{99};
    require(locality_exercises::chase(terminal, 0, 1) == 0, "final successor not dereferenced");
  });
  checks.run("layout", [] {
    const std::array<locality_exercises::Record, 3> records{{{7, 999, 1}, {11, 333, 2}, {0, 1, 3}}};
    const std::array<std::uint64_t, 3> prices{7, 11, 0};
    require(locality_exercises::sum_prices_aos(records) == 18, "AoS selects price only");
    require(locality_exercises::sum_prices_soa(prices) == 18, "SoA agrees");
    require(locality_exercises::sum_prices_aos({}) == 0 &&
                locality_exercises::sum_prices_soa({}) == 0,
            "empty layouts");
    const std::array<std::uint32_t, 2> wide{0xffffffffU, 0xffffffffU};
    const std::array<std::size_t, 2> order{0, 1};
    require(locality_exercises::sum_ordered(wide, order) == 8589934590ULL, "64-bit accumulation");
  });
  checks.run("bounds", [] {
    bool order_threw = false, stride_threw = false, chase_threw = false;
    const std::array<std::uint32_t, 1> values{1};
    const std::array<std::size_t, 1> invalid{1};
    try {
      (void)locality_exercises::sum_ordered(values, invalid);
    } catch (const std::out_of_range&) {
      order_threw = true;
    }
    try {
      (void)locality_exercises::sum_stride(values, 0);
    } catch (const std::invalid_argument&) {
      stride_threw = true;
    }
    try {
      (void)locality_exercises::chase(invalid, 0, 2);
    } catch (const std::out_of_range&) {
      chase_threw = true;
    }
    require(order_threw && stride_threw && chase_threw, "invalid accesses are rejected");
  });
  return checks.finish();
}
