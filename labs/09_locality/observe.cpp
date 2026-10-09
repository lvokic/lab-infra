#include "lab_check.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <exception>
#include <functional>
#include <iostream>
#include <numeric>
#include <random>
#include <vector>

namespace {
struct Record {
  std::uint64_t price, quantity, timestamp;
};

template <typename Function>
long long measure(const char* name, Function operation, std::uint64_t expected) {
  const auto started = std::chrono::steady_clock::now();
  const auto checksum = operation();
  const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
                           std::chrono::steady_clock::now() - started)
                           .count();
  require(checksum == expected, "equal work has equal checksum");
  std::cout << '[' << name << "] ns=" << elapsed << " checksum=" << checksum << '\n';
  return elapsed;
}
}  // namespace

// 观察程序用标准算法作为已完成基线；练习自己实现访问路径，不调用这些算法代替 TODO。
int main() {
  try {
    constexpr std::size_t count = 32768;
    std::vector<std::uint32_t> values(count);
    std::iota(values.begin(), values.end(), 1U);
    std::vector<std::size_t> order(count);
    std::iota(order.begin(), order.end(), std::size_t{0});
    std::mt19937 random(42);
    std::shuffle(order.begin(), order.end(), random);
    std::vector<Record> records(count);
    std::vector<std::uint64_t> prices(count);
    for (std::size_t index = 0; index < count; ++index) {
      records[index] = {values[index], 3, index};
      prices[index] = values[index];
    }
    const auto expected = static_cast<std::uint64_t>(count) * (count + 1) / 2;
    const auto sequential = [&] {
      return std::accumulate(values.begin(), values.end(), std::uint64_t{0});
    };
    const auto indirect = [&] {
      return std::transform_reduce(order.begin(), order.end(), std::uint64_t{0}, std::plus<>{},
                                   [&](std::size_t index) {
                                     return values[index];
                                   });
    };
    const auto aos = [&] {
      return std::transform_reduce(records.begin(), records.end(), std::uint64_t{0}, std::plus<>{},
                                   [](const Record& record) {
                                     return record.price;
                                   });
    };
    const auto soa = [&] {
      return std::accumulate(prices.begin(), prices.end(), std::uint64_t{0});
    };
    require(sequential() == expected && indirect() == expected && aos() == expected &&
                soa() == expected,
            "warmup checksums");
    std::cout << "[layout] count=" << count << " value_bytes=" << values.size() * sizeof(values[0])
              << " index_bytes=" << order.size() * sizeof(order[0])
              << " aos_bytes=" << records.size() * sizeof(Record)
              << " soa_price_bytes=" << prices.size() * sizeof(prices[0])
              << " sizeof_record=" << sizeof(Record) << '\n';
    std::array<long long, 3> seq_times{}, indirect_times{}, aos_times{}, soa_times{};
    for (std::size_t round = 0; round < 3; ++round) {
      if (round % 2 == 0) {
        seq_times[round] = measure("sequential", sequential, expected);
        indirect_times[round] = measure("permutation", indirect, expected);
        aos_times[round] = measure("AoS-price", aos, expected);
        soa_times[round] = measure("SoA-price", soa, expected);
      } else {
        soa_times[round] = measure("SoA-price", soa, expected);
        aos_times[round] = measure("AoS-price", aos, expected);
        indirect_times[round] = measure("permutation", indirect, expected);
        seq_times[round] = measure("sequential", sequential, expected);
      }
    }
    for (auto* samples : {&seq_times, &indirect_times, &aos_times, &soa_times})
      std::sort(samples->begin(), samples->end());
    std::cout << "[median-ns] sequential=" << seq_times[1] << " permutation=" << indirect_times[1]
              << " AoS=" << aos_times[1] << " SoA=" << soa_times[1] << '\n';
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
