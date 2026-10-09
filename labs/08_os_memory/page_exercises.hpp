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
  (void)bytes;
  (void)page_size;
  throw std::logic_error("TODO: page_offsets");
}

// 只写每页首字节，其他字节不变；返回实际写入次数。
inline std::size_t touch_pages(std::span<std::byte> bytes, std::size_t page_size, std::byte value) {
  (void)bytes;
  (void)page_size;
  (void)value;
  throw std::logic_error("TODO: touch_pages");
}

// 累加每页首字节的无符号值；page_size==0 同样抛 invalid_argument。
inline std::uint64_t checksum_pages(std::span<const std::byte> bytes, std::size_t page_size) {
  (void)bytes;
  (void)page_size;
  throw std::logic_error("TODO: checksum_pages");
}
}  // namespace page_exercises
