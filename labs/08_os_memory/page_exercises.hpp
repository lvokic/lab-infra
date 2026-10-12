#pragma once
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <vector>

namespace page_exercises {
// 返回区域每一页首字节的偏移；最后不足一页也包含。bytes==0 返回空。
// page_size==0 抛 invalid_argument；不要用 bytes+page_size-1，避免溢出。
inline std::vector<std::size_t> page_offsets(std::size_t bytes, std::size_t page_size) {
  if (page_size == 0)
    throw std::invalid_argument("invalid");
  if (bytes == 0)
    return {};
  const std::size_t pages = 1 + (bytes - 1) / page_size;
  std::vector<std::size_t> res(pages);
  for (std::size_t i = 0; i < pages; ++i) {
    res[i] = page_size * i;
  }
  return res;
}

// 只写每页首字节，其他字节不变；返回实际写入次数。
inline std::size_t touch_pages(std::span<std::byte> bytes, std::size_t page_size, std::byte value) {
  if (page_size == 0)
    throw std::invalid_argument("invalid");
  if (bytes.empty())
    return 0;
  const std::size_t pages = 1 + (bytes.size() - 1) / page_size;
  for (std::size_t i = 0; i < pages; ++i) {
    bytes[i * page_size] = value;
  }
  return pages;
}

// 累加每页首字节的无符号值；page_size==0 同样抛 invalid_argument。
inline std::uint64_t checksum_pages(std::span<const std::byte> bytes, std::size_t page_size) {
  if (page_size == 0)
    throw std::invalid_argument("invalid");
  if (bytes.empty())
    return 0;
  const std::size_t pages = 1 + (bytes.size() - 1) / page_size;
  std::uint64_t sum = 0;
  for (std::size_t i = 0; i < pages; ++i) {
    sum += std::to_integer<unsigned int>(bytes[i * page_size]);
  }
  return sum;
}
}  // namespace page_exercises
