#include "lab_check.hpp"
#include "mapped_region.hpp"
#include "page_exercises.hpp"
#include <chrono>
#include <exception>
#include <iostream>
#include <limits>
#include <sys/resource.h>

namespace {

struct Sample {
  std::chrono::steady_clock::time_point time;
  long minor;
  long major;
};

Sample sample() {
  rusage usage{};
  if (::getrusage(RUSAGE_SELF, &usage) != 0)
    throw std::system_error(errno, std::generic_category(), "getrusage");
  return {std::chrono::steady_clock::now(), usage.ru_minflt, usage.ru_majflt};
}

void report(const char* stage, Sample before, Sample after) {
  std::cout
      << '[' << stage << "] us="
      << std::chrono::duration_cast<std::chrono::microseconds>(after.time - before.time).count()
      << " minor_fault_delta=" << after.minor - before.minor
      << " major_fault_delta=" << after.major - before.major << '\n';
}

}

// 测量支撑已经写好；实际访问只调用你的 TODO 接口，不提供循环答案。
int main() {
  try {
    constexpr std::size_t pages = 256;
    const auto page_size = system_page_size();
    require(page_size <= std::numeric_limits<std::size_t>::max() / pages, "size fits size_t");
    const auto before = sample();
    MappedRegion region(page_size * pages);
    const auto mapped = sample();
    const auto first_count = page_exercises::touch_pages(region.bytes(), page_size, std::byte{1});
    const auto first = sample();
    const auto second_count = page_exercises::touch_pages(region.bytes(), page_size, std::byte{2});
    const auto second = sample();
    const auto checksum = page_exercises::checksum_pages(region.bytes(), page_size);
    require(first_count == pages && second_count == pages && checksum == pages * 2,
            "learner page writes and checksum agree");
    std::cout << "[learner pages] page_size=" << page_size
              << " virtual_bytes=" << region.bytes().size() << " bytes_written_per_pass=" << pages
              << " checksum=" << checksum << '\n';
    report("mmap", before, mapped);
    report("first-page-write", mapped, first);
    report("second-page-write", first, second);
    std::cout << "Process fault counters and elapsed time do not identify TLB misses.\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "[FAIL] " << error.what() << '\n';
    return 1;
  }
}
