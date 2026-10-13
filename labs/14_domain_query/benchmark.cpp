#include "lab_check.hpp"
#include "query_exercises.hpp"
#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <exception>
#include <iostream>
#include <numeric>
#include <string_view>
#include <system_error>
#include <vector>

namespace {
template <typename Function>
long long measure(const char* label, Function operation, std::uint64_t expected) {
  const auto started = std::chrono::steady_clock::now();
  const auto result = operation();
  const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
                           std::chrono::steady_clock::now() - started)
                           .count();
  require(result == expected, "query benchmark checksum");
  std::cout << '[' << label << "] ns=" << elapsed << " result=" << result << '\n';
  return elapsed;
}
}  // namespace

int main(int argc, char** argv) {
  constexpr std::string_view usage = "Usage: domain_query_benchmark [--batch 1..4096|--help]\n";
  std::size_t batch = 64;
  if (argc == 2 && std::string_view(argv[1]) == "--help") {
    std::cout << usage;
    return 0;
  }
  if (argc != 1) {
    if (argc != 3 || std::string_view(argv[1]) != "--batch") {
      std::cerr << usage;
      return 2;
    }
    const std::string_view argument(argv[2]);
    const auto converted =
        std::from_chars(argument.data(), argument.data() + argument.size(), batch);
    if (converted.ec != std::errc{} || converted.ptr != argument.data() + argument.size() ||
        batch == 0 || batch > 4096) {
      std::cerr << usage;
      return 2;
    }
  }
  try {
    constexpr std::size_t count = 32768;
    const auto plan =
        domain_query::compile_query("SUM price WHERE quantity >= 2 AND timestamp < 20000");
    std::vector<domain_query::Record> rows(count);
    std::vector<std::uint64_t> prices(count), quantities(count), timestamps(count);
    for (std::size_t index = 0; index < count; ++index) {
      rows[index] = {1 + index % 97, index % 4, index};
      prices[index] = rows[index].price;
      quantities[index] = rows[index].quantity;
      timestamps[index] = rows[index].timestamp;
    }
    const domain_query::Columns columns{prices, quantities, timestamps};
    const auto expected = std::accumulate(
        rows.begin(), rows.end(), std::uint64_t{0}, [](std::uint64_t total, const auto& row) {
          return total + (row.quantity >= 2 && row.timestamp < 20000 ? row.price : 0);
        });
    const auto aos = [&] {
      return domain_query::execute_aos(rows, plan);
    };
    const auto soa = [&] {
      return domain_query::execute_soa(columns, plan, batch);
    };
    require(aos() == expected && soa() == expected, "query warmup checksums");
    std::cout << "[configuration] rows=" << count << " batch_size=" << batch
              << " AoS_bytes=" << rows.size() * sizeof(rows[0])
              << " SoA_bytes=" << count * 3 * sizeof(std::uint64_t) << '\n';
    std::array<long long, 3> row_samples{}, column_samples{};
    for (std::size_t round = 0; round < row_samples.size(); ++round) {
      if (round % 2 == 0) {
        row_samples[round] = measure("AoS", aos, expected);
        column_samples[round] = measure("SoA-batches", soa, expected);
      } else {
        column_samples[round] = measure("SoA-batches", soa, expected);
        row_samples[round] = measure("AoS", aos, expected);
      }
    }
    std::sort(row_samples.begin(), row_samples.end());
    std::sort(column_samples.begin(), column_samples.end());
    std::cout << "[median-ns] AoS=" << row_samples[1] << " SoA=" << column_samples[1] << '\n';
    std::cout << "Parsing, data generation and layout conversion are outside timed execution.\n"
              << "No assertion requires batching or column layout to be faster.\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
