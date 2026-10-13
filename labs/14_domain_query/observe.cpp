#include "lab_check.hpp"
#include "lab_exercise_checks.hpp"
#include "query_exercises.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <functional>
#include <iostream>
#include <iterator>
#include <numeric>
#include <vector>

namespace {
using domain_query::Record;
constexpr std::array<Record, 4> records{{{10, 1, 100}, {20, 3, 200}, {30, 4, 300}, {40, 2, 400}}};

void plan() {
  // 手工准备 tokens/plan，让你观察阶段；没有提供 tokenize/compile_query 的实现。
  const std::array<const char*, 10> tokens{
      "SUM", "price", "WHERE", "quantity", ">=", "2", "AND", "timestamp", "<", "350"};
  std::cout << "[query] SUM price WHERE quantity >= 2 AND timestamp < 350\n[tokens]";
  for (const auto* token : tokens)
    std::cout << ' ' << token;
  std::cout << "\n[plan] aggregate=SUM field=price predicates=2 conjunction=AND\n";
  std::vector<Record> selected;
  std::copy_if(records.begin(), records.end(), std::back_inserter(selected), [](const Record& row) {
    return row.quantity >= 2 && row.timestamp < 350;
  });
  const auto sum = std::transform_reduce(selected.begin(), selected.end(), std::uint64_t{0},
                                         std::plus<>{}, [](const Record& row) {
                                           return row.price;
                                         });
  require(selected.size() == 2 && sum == 50, "standard-library oracle for the shown query");
  std::cout << "[result] matched_rows=" << selected.size() << " SUM_price=" << sum << '\n';
  std::cout << "Tokens and plan above are prepared examples; the learner writes the parser.\n";
}

void layout() {
  std::vector<std::uint64_t> prices, quantities, timestamps;
  for (const auto& row : records) {
    prices.push_back(row.price);
    quantities.push_back(row.quantity);
    timestamps.push_back(row.timestamp);
  }
  const auto aos_sum = std::transform_reduce(records.begin(), records.end(), std::uint64_t{0},
                                             std::plus<>{}, [](const Record& row) {
                                               return row.price;
                                             });
  const auto soa_sum = std::accumulate(prices.begin(), prices.end(), std::uint64_t{0});
  require(aos_sum == 100 && soa_sum == aos_sum, "row and column baselines agree");
  std::cout << "[layout] rows=" << records.size() << " sizeof_record=" << sizeof(Record)
            << " AoS_bytes=" << sizeof(records)
            << " price_column_bytes=" << prices.size() * sizeof(prices[0]) << " all_column_bytes="
            << (prices.size() + quantities.size() + timestamps.size()) * sizeof(std::uint64_t)
            << " SUM_price=" << soa_sum << '\n';
  std::cout << "SUM price needs price; WHERE quantity >= 2 additionally needs quantity.\n";
}

void batching() {
  const std::array<std::uint64_t, 5> prices{10, 20, 30, 40, 50};
  constexpr std::size_t batch_size = 2;
  std::uint64_t sum = 0;
  for (std::size_t begin = 0; begin < prices.size(); begin += batch_size) {
    const auto end = std::min(begin + batch_size, prices.size());
    const auto subtotal =
        std::accumulate(prices.begin() + static_cast<std::ptrdiff_t>(begin),
                        prices.begin() + static_cast<std::ptrdiff_t>(end), std::uint64_t{0});
    sum += subtotal;
    std::cout << "[batch] rows=[" << begin << ',' << end << ") subtotal=" << subtotal << '\n';
  }
  require(sum == 150, "partial final batch contributes to the result");
  std::cout << "[batch-result] SUM_price=" << sum << "; batching alone does not imply SIMD.\n";
}

}  // namespace

int main(int argc, char** argv) {
  LabExerciseChecks observations(argc, argv);
  observations.run("plan", plan);
  observations.run("layout", layout);
  observations.run("batch", batching);
  return observations.finish();
}
