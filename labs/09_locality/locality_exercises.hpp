#pragma once
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>

namespace locality_exercises {
struct Record {
  std::uint64_t price = 0;
  std::uint64_t quantity = 0;
  std::uint64_t timestamp = 0;
};

// 按 order 中的下标累加 values；允许重复下标，空 order 返回 0。
// 任一下标越界抛 out_of_range。累加使用 uint64_t。
inline std::uint64_t sum_ordered(std::span<const std::uint32_t> values,
                                 std::span<const std::size_t> order) {
  (void)values;
  (void)order;
  throw std::logic_error("TODO: sum_ordered");
}

// 访问 0, stride, 2*stride ...，只累加范围内的元素。
// stride==0 抛 invalid_argument；循环不要因无符号加法溢出而重新开始。
inline std::uint64_t sum_stride(std::span<const std::uint32_t> values, std::size_t stride) {
  (void)values;
  (void)stride;
  throw std::logic_error("TODO: sum_stride");
}

// 从 start 开始恰好 steps 次：累加当前节点下标，然后沿 next[current] 前进。
// steps==0 返回 0，无需访问 start；访问的当前节点越界则抛 out_of_range。
// 最后一轮也读取 next[current]，但不解引用得到的后继，故后继可以指向范围外。
inline std::uint64_t chase(std::span<const std::size_t> next, std::size_t start,
                           std::size_t steps) {
  (void)next;
  (void)start;
  (void)steps;
  throw std::logic_error("TODO: chase");
}

// AoS：只累加 price，不把 quantity/timestamp 加入结果。
inline std::uint64_t sum_prices_aos(std::span<const Record> records) {
  (void)records;
  throw std::logic_error("TODO: sum_prices_aos");
}

// SoA：这里只传所需字段；其余字段不属于访问接口。
inline std::uint64_t sum_prices_soa(std::span<const std::uint64_t> prices) {
  (void)prices;
  throw std::logic_error("TODO: sum_prices_soa");
}
}  // namespace locality_exercises
