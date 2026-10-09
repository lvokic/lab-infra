#pragma once
#include <cstddef>
#include <span>
#include <stdexcept>
#include <system_error>
#include <utility>
#include <cerrno>
#include <sys/mman.h>
#include <unistd.h>

// 已完成的系统调用包装；本 lab 的 TODO 位于 page_exercises.hpp。
inline std::size_t system_page_size() {
  const long result = ::sysconf(_SC_PAGESIZE);
  if (result <= 0)
    throw std::runtime_error("sysconf(_SC_PAGESIZE) failed");
  return static_cast<std::size_t>(result);
}

class MappedRegion {
public:
  explicit MappedRegion(std::size_t bytes) : bytes_(bytes) {
    if (bytes == 0)
      return;
    void* result =
        ::mmap(nullptr, bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (result == MAP_FAILED)
      throw std::system_error(errno, std::generic_category(), "mmap");
    data_ = static_cast<std::byte*>(result);
  }

  ~MappedRegion() noexcept {
    release();
  }

  MappedRegion(const MappedRegion&) = delete;
  MappedRegion& operator=(const MappedRegion&) = delete;

  MappedRegion(MappedRegion&& other) noexcept
      : data_(std::exchange(other.data_, nullptr)), bytes_(std::exchange(other.bytes_, 0)) {}

  MappedRegion& operator=(MappedRegion&& other) noexcept {
    if (this != &other) {
      release();
      data_ = std::exchange(other.data_, nullptr);
      bytes_ = std::exchange(other.bytes_, 0);
    }
    return *this;
  }

  std::span<std::byte> bytes() noexcept {
    return {data_, bytes_};
  }

  std::span<const std::byte> bytes() const noexcept {
    return {data_, bytes_};
  }

private:
  void release() noexcept {
    if (data_ != nullptr)
      ::munmap(data_, bytes_);
    data_ = nullptr;
    bytes_ = 0;
  }

  std::byte* data_ = nullptr;
  std::size_t bytes_ = 0;
};
