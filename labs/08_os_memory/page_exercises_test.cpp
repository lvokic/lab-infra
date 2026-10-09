#include "lab_check.hpp"
#include "lab_exercise_checks.hpp"
#include "mapped_region.hpp"
#include "page_exercises.hpp"
#include <array>
#include <limits>
#include <utility>

int main(int argc, char** argv) {
  LabExerciseChecks checks(argc, argv);
  checks.run("offsets", [] {
    using page_exercises::page_offsets;
    require(page_offsets(0, 4).empty(), "empty region has no page offsets");
    require(page_offsets(1, 4) == std::vector<std::size_t>{0}, "one partial page");
    require(page_offsets(8, 4) == std::vector<std::size_t>{0, 4}, "exact page multiple");
    require(page_offsets(9, 4) == std::vector<std::size_t>{0, 4, 8}, "last partial page");
    require(page_offsets(7, 1).size() == 7, "one-byte model pages");
    const auto maximum = std::numeric_limits<std::size_t>::max();
    require(page_offsets(maximum, maximum) == std::vector<std::size_t>{0}, "large exact page");
    require(page_offsets(maximum, maximum - 1) == std::vector<std::size_t>{0, maximum - 1},
            "large final partial page avoids unsigned wrap");
  });
  checks.run("invalid", [] {
    bool offsets_threw = false, touch_threw = false, checksum_threw = false;
    try {
      (void)page_exercises::page_offsets(1, 0);
    } catch (const std::invalid_argument&) {
      offsets_threw = true;
    }
    try {
      (void)page_exercises::touch_pages({}, 0, std::byte{1});
    } catch (const std::invalid_argument&) {
      touch_threw = true;
    }
    try {
      (void)page_exercises::checksum_pages({}, 0);
    } catch (const std::invalid_argument&) {
      checksum_threw = true;
    }
    require(offsets_threw && touch_threw && checksum_threw, "zero page size is rejected");
  });
  checks.run("touch", [] {
    std::array<std::byte, 10> data{};
    data.fill(std::byte{7});
    require(page_exercises::touch_pages(data, 4, std::byte{3}) == 3, "three writes");
    for (std::size_t index = 0; index < data.size(); ++index)
      require(data[index] == (index % 4 == 0 ? std::byte{3} : std::byte{7}),
              "only page starts are changed");
    require(page_exercises::checksum_pages(data, 4) == 9, "checksum reads page starts");
    require(page_exercises::touch_pages({}, 4, std::byte{3}) == 0, "empty writes");
    require(page_exercises::checksum_pages({}, 4) == 0, "empty checksum");
  });
  checks.run("mapping", [] {
    const auto page = system_page_size();
    MappedRegion region(page + 3);
    require(page_exercises::touch_pages(region.bytes(), page, std::byte{5}) == 2,
            "real mapping includes final partial page");
    require(page_exercises::checksum_pages(region.bytes(), page) == 10, "mapped checksum");
    const auto address = region.bytes().data();
    MappedRegion moved(std::move(region));
    require(region.bytes().empty() && moved.bytes().data() == address, "move transfers mapping");
    MappedRegion target(page);
    target = std::move(moved);
    require(moved.bytes().empty() && target.bytes().data() == address,
            "assignment transfers mapping");
    MappedRegion empty(0);
    require(empty.bytes().empty(), "zero-sized wrapper performs no mmap");
  });
  return checks.finish();
}
