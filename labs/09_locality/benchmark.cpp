#include "lab_check.hpp"
#include "locality_exercises.hpp"
#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <exception>
#include <functional>
#include <iostream>
#include <numeric>
#include <random>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace {
constexpr std::size_t maximum_count = 262144;

struct Options {
  std::size_t count = 32768;
  std::size_t stride = 8;
};

void usage() {
  std::cout << "Usage: locality_benchmark [--count N] [--stride N]\n"
               "       locality_benchmark --help\n"
               "count/stride: 1..262144; defaults: count=32768 stride=8\n"
               "Complete locality_exercises.hpp first; checksums are verified before timing.\n";
}

std::size_t number(std::string_view text) {
  std::size_t value = 0;
  const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
  if (error != std::errc{} || end != text.data() + text.size() || value == 0 ||
      value > maximum_count)
    throw std::invalid_argument("count and stride must be integers in 1..262144");
  return value;
}

Options parse(int argc, char** argv) {
  Options result;
  bool count_seen = false, stride_seen = false;
  for (int index = 1; index < argc; index += 2) {
    if (index + 1 >= argc)
      throw std::invalid_argument("option requires a value");
    const std::string_view option = argv[index];
    if (option == "--count" && !count_seen) {
      result.count = number(argv[index + 1]);
      count_seen = true;
    } else if (option == "--stride" && !stride_seen) {
      result.stride = number(argv[index + 1]);
      stride_seen = true;
    } else {
      throw std::invalid_argument("unknown or repeated option");
    }
  }
  return result;
}

struct Scene {
  const char* name;
  std::size_t visits;
  std::uint64_t expected;
  std::function<std::uint64_t()> operation;
  std::array<long long, 3> samples{};
};
}  // namespace

// 只提供数据准备、测量与校验；所有访问路径调用学员 header，未实现时明确失败。
int main(int argc, char** argv) {
  if (argc == 2 && std::string_view(argv[1]) == "--help") {
    usage();
    return 0;
  }
  Options options;
  try {
    options = parse(argc, argv);
  } catch (const std::invalid_argument& error) {
    std::cerr << error.what() << '\n';
    usage();
    return 2;
  }
  try {
    using namespace locality_exercises;
    const auto count = options.count;
    std::vector<std::uint32_t> values(count);
    std::iota(values.begin(), values.end(), 1U);
    std::vector<std::size_t> sequential(count), permutation(count), next(count);
    std::iota(sequential.begin(), sequential.end(), std::size_t{0});
    permutation = sequential;
    std::mt19937 random(42);
    std::shuffle(permutation.begin(), permutation.end(), random);
    // 数据生成：排列定义一个完整单环，不在这里实现依赖访问/求和。
    for (std::size_t index = 0; index < count; ++index)
      next[permutation[index]] = permutation[(index + 1) % count];
    std::vector<Record> records(count);
    std::vector<std::uint64_t> prices(count);
    for (std::size_t index = 0; index < count; ++index) {
      records[index] = {values[index], 17, index};
      prices[index] = values[index];
    }
    const auto total = static_cast<std::uint64_t>(count) * (count + 1) / 2;
    const auto chase_total = static_cast<std::uint64_t>(count) * (count - 1) / 2;
    const auto stride_visits = 1 + (count - 1) / options.stride;
    const auto stride_total =
        static_cast<std::uint64_t>(stride_visits) +
        static_cast<std::uint64_t>(options.stride) * stride_visits * (stride_visits - 1) / 2;
    std::array<Scene, 6> scenes{{
        {"sequential-indices", count, total,
         [&] {
           return sum_ordered(values, sequential);
         }},
        {"permutation-indices", count, total,
         [&] {
           return sum_ordered(values, permutation);
         }},
        {"stride", stride_visits, stride_total,
         [&] {
           return sum_stride(values, options.stride);
         }},
        {"chase-single-cycle", count, chase_total,
         [&] {
           return chase(next, permutation.front(), count);
         }},
        {"AoS-price", count, total,
         [&] {
           return sum_prices_aos(records);
         }},
        {"SoA-price", count, total,
         [&] {
           return sum_prices_soa(prices);
         }},
    }};
    std::cout << "[config] count=" << count << " stride=" << options.stride
              << " rounds=3 seed=42 value_bytes=" << values.size() * sizeof(values[0])
              << " each_index_array_bytes=" << sequential.size() * sizeof(sequential[0])
              << " aos_bytes=" << records.size() * sizeof(Record)
              << " soa_price_bytes=" << prices.size() * sizeof(prices[0]) << '\n';
    for (auto& scene : scenes) {
      const auto checksum = scene.operation();
      require(checksum == scene.expected, "warmup checksum differs from expected");
      std::cout << "[warmup " << scene.name << "] visits=" << scene.visits
                << " checksum=" << checksum << '\n';
    }
    for (std::size_t round = 0; round < 3; ++round) {
      for (std::size_t offset = 0; offset < scenes.size(); ++offset) {
        auto& scene = scenes[round % 2 == 0 ? offset : scenes.size() - 1 - offset];
        const auto started = std::chrono::steady_clock::now();
        const auto checksum = scene.operation();
        scene.samples[round] = std::chrono::duration_cast<std::chrono::nanoseconds>(
                                   std::chrono::steady_clock::now() - started)
                                   .count();
        require(checksum == scene.expected, "timed checksum differs from expected");
        std::cout << "[round " << round + 1 << ' ' << scene.name << "] visits=" << scene.visits
                  << " checksum=" << checksum << " ns=" << scene.samples[round] << '\n';
      }
    }
    for (auto& scene : scenes) {
      std::sort(scene.samples.begin(), scene.samples.end());
      std::cout << "[median " << scene.name << "] ns=" << scene.samples[1]
                << " visits=" << scene.visits << '\n';
    }
  } catch (const std::exception& error) {
    std::cerr << "Benchmark failed: " << error.what() << '\n';
    return 1;
  }
}
