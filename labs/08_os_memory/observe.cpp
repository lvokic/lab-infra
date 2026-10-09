#include "lab_check.hpp"
#include "mapped_region.hpp"
#include <chrono>
#include <cstdint>
#include <cstring>
#include <exception>
#include <iostream>
#include <memory>
#include <system_error>
#include <thread>
#include <sys/resource.h>

namespace {
struct Snapshot {
  std::chrono::steady_clock::time_point time;
  long minor;
  long major;
};

Snapshot snapshot() {
  rusage usage{};
  if (::getrusage(RUSAGE_SELF, &usage) != 0)
    throw std::system_error(errno, std::generic_category(), "getrusage");
  return {std::chrono::steady_clock::now(), usage.ru_minflt, usage.ru_majflt};
}

void report(const char* stage, Snapshot before, Snapshot after) {
  std::cout
      << '[' << stage << "] elapsed_us="
      << std::chrono::duration_cast<std::chrono::microseconds>(after.time - before.time).count()
      << " minor_fault_delta=" << after.minor - before.minor
      << " major_fault_delta=" << after.major - before.major << '\n';
}
}  // namespace

// 已完成的观察程序；先运行，再独立完成 page_exercises.hpp。
int main() {
  try {
    int main_local = 0;
    auto heap_value = std::make_unique<int>(42);
    std::jthread worker([&] {
      int worker_local = 0;
      std::cout << "[thread] stack_local=" << &worker_local << " shared_heap=" << heap_value.get()
                << " value=" << *heap_value << '\n';
    });
    worker.join();
    std::cout << "[main] stack_local=" << &main_local << " shared_heap=" << heap_value.get()
              << '\n';
    const auto page = system_page_size();
    constexpr std::size_t pages = 256;
    require(page <= static_cast<std::size_t>(-1) / pages, "mapping size fits size_t");
    const auto start = snapshot();
    MappedRegion region(page * pages);
    const auto mapped = snapshot();
    // 观察程序使用库函数写整个区域。练习独立实现“每页只写一字节”。
    std::memset(region.bytes().data(), 1, region.bytes().size());
    const auto first = snapshot();
    std::memset(region.bytes().data(), 2, region.bytes().size());
    const auto second = snapshot();
    std::uint64_t checksum = 0;
    for (std::size_t offset = 0; offset < region.bytes().size(); offset += page)
      checksum += std::to_integer<unsigned int>(region.bytes()[offset]);
    require(checksum == pages * 2, "second pass writes are observable");
    std::cout << "[mapping] page_size=" << page << " virtual_bytes=" << region.bytes().size()
              << " bytes_written_per_pass=" << region.bytes().size() << " checksum=" << checksum
              << '\n';
    report("mmap", start, mapped);
    report("first-write", mapped, first);
    report("second-write", first, second);
    std::cout << "Fault counters cover the whole process; timing is not a TLB measurement.\n";
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
