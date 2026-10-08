#include "check_runner.hpp"
#include "lab_check.hpp"
#include "mini_hash_map.hpp"
#include <algorithm>
#include <random>
#include <unordered_map>

namespace {
struct ConstantHash {
  std::size_t operator()(int) const noexcept {
    return 0;
  }
};

template <typename Hash>
void check_against(const MiniHashMap<Hash>& actual,
                   const std::unordered_map<int, std::string>& expected) {
  require(actual.size() == expected.size(), "size matches reference");
  require(actual.bucket_count() > 0 && actual.size() <= actual.bucket_count(),
          "positive bucket count and fixed maximum load factor 1");
  for (int key = -5; key <= 25; ++key) {
    const auto* found = actual.find(key);
    const auto reference = expected.find(key);
    require((found != nullptr) == (reference != expected.end()), "key presence matches");
    if (found != nullptr)
      require(*found == reference->second, "mapped value matches");
  }
}
}  // namespace

int main(int argc, char** argv) {
  static_assert(!std::is_copy_constructible_v<MiniHashMap<>>);
  static_assert(
      std::is_same_v<decltype(std::declval<const MiniHashMap<>&>().find(0)), const std::string*>);
  ContainerChecks checks(argc, argv);

  checks.run("empty", [] {
    MiniHashMap<> values(4);
    require(values.empty() && values.size() == 0 && values.bucket_count() >= 4 &&
                values.load_factor() == 0.0F,
            "initial state is empty");
    bool rejected = false;
    try {
      MiniHashMap<> invalid(0);
    } catch (const std::invalid_argument&) {
      rejected = true;
    }
    require(rejected, "zero initial bucket count is rejected");
  });

  checks.run("insert_find", [] {
    MiniHashMap<> values(4);
    require(values.insert(10, "A") && values.insert(20, "B"), "insert distinct keys");
    require(!values.insert(10, "replacement") && values.size() == 2,
            "duplicate key does not create an element");
    auto* found = values.find(10);
    require(found != nullptr && *found == "A", "duplicate insertion preserves value");
    *found = "updated";
    const auto& constant = values;
    require(constant.find(10) != nullptr && *constant.find(10) == "updated",
            "const lookup sees updates through mutable lookup");
    require(values.find(99) == nullptr && constant.find(99) == nullptr && values.size() == 2,
            "missing lookup does not insert");
  });

  checks.run("collisions", [] {
    MiniHashMap<ConstantHash> values(4);
    for (int key = 0; key < 16; ++key) {
      require(values.insert(key, std::to_string(key)), "insert colliding distinct keys");
    }
    std::size_t occupied = 0;
    std::size_t largest = 0;
    std::size_t total = 0;
    for (std::size_t index = 0; index < values.bucket_count(); ++index) {
      const auto length = values.bucket_size(index);
      if (length != 0)
        ++occupied;
      largest = std::max(largest, length);
      total += length;
    }
    require(occupied == 1 && largest == 16 && total == values.size(),
            "constant hash keeps all distinct keys in one bucket");
    require(values.find(-1) == nullptr, "missing lookup terminates in a crowded bucket");
    auto* kept = values.find(7);
    require(kept != nullptr, "saved collision element exists");
    values.rehash(values.bucket_count() + 17);
    require(values.find(7) == kept, "rehash preserves collision element addresses");
    for (int key : {15, 8, 0})
      require(values.erase(key), "erase colliding keys");
    for (int key = 0; key < 16; ++key) {
      const auto* found = values.find(key);
      if (key == 15 || key == 8 || key == 0) {
        require(found == nullptr, "erased collision key is absent");
      } else {
        require(found != nullptr && *found == std::to_string(key),
                "remaining collision keys are reachable");
      }
    }
  });

  checks.run("reserve_rehash", [] {
    MiniHashMap<> values(2);
    require(values.insert(10, "A") && values.insert(20, "B"), "prepare elements");
    auto* kept = values.find(10);
    require(kept != nullptr, "saved element exists");
    values.reserve(32);
    require(values.size() == 2 && values.bucket_count() >= 32 && values.find(10) == kept &&
                *kept == "A",
            "reserve prepares total capacity and preserves addresses");
    const auto buckets = values.bucket_count();
    values.reserve(1);
    require(values.bucket_count() == buckets, "reserve does not shrink this MiniHashMap");
    values.rehash(1);
    require(values.bucket_count() >= values.size() && values.find(10) == kept,
            "small rehash still satisfies the load bound");
    const auto requested = values.bucket_count() + 7;
    values.rehash(requested);
    require(values.bucket_count() >= requested && values.size() == 2 && values.find(10) == kept,
            "large rehash changes buckets and preserves elements");
    bool rejected = false;
    try {
      static_cast<void>(values.bucket_size(values.bucket_count()));
    } catch (const std::out_of_range&) {
      rejected = true;
    }
    require(rejected, "bucket_size rejects an invalid index");
  });

  checks.run("growth", [] {
    MiniHashMap<> values(1);
    require(values.insert(0, "0"), "insert from a single bucket");
    auto* first = values.find(0);
    require(first != nullptr, "saved first element exists");
    for (int key = 1; key < 128; ++key) {
      require(values.insert(key, std::to_string(key)), "insert during repeated growth");
      require(values.size() == static_cast<std::size_t>(key + 1) &&
                  values.size() <= values.bucket_count() && values.find(0) == first,
              "growth preserves size, load bound and existing addresses");
    }
    for (int key = 0; key < 128; ++key) {
      const auto* found = values.find(key);
      require(found != nullptr && *found == std::to_string(key), "growth retains every key");
    }
  });

  checks.run("erase", [] {
    MiniHashMap<> values;
    require(!values.erase(99), "erasing from an empty table returns false");
    for (int key = 0; key < 8; ++key)
      values.insert(key, std::to_string(key));
    auto* kept = values.find(0);
    require(kept != nullptr, "saved survivor exists");
    require(values.erase(3) && !values.erase(3) && values.size() == 7 &&
                values.find(3) == nullptr && values.find(0) == kept && *kept == "0",
            "erase removes exactly one key and preserves other addresses");
  });

  checks.run("clear_reuse", [] {
    MiniHashMap<> values;
    values.reserve(16);
    for (int key = 0; key < 8; ++key)
      values.insert(key, std::to_string(key));
    const auto buckets = values.bucket_count();
    values.clear();
    values.clear();
    require(values.empty() && values.bucket_count() == buckets && values.find(0) == nullptr,
            "repeated clear empties the table while retaining buckets");
    for (std::size_t index = 0; index < buckets; ++index) {
      require(values.bucket_size(index) == 0, "clear empties every chain");
    }
    require(values.insert(-1, "reused") && values.find(-1) != nullptr &&
                *values.find(-1) == "reused",
            "cleared table supports reuse");
  });

  checks.run("random", [] {
    MiniHashMap<ConstantHash> actual(2);
    std::unordered_map<int, std::string> expected;
    std::mt19937 random(404);
    std::uniform_int_distribution<int> operation(0, 7);
    std::uniform_int_distribution<int> keys(-5, 25);
    std::uniform_int_distribution<int> capacity(0, 40);
    for (int step = 0; step < 400; ++step) {
      const int key = keys(random);
      switch (operation(random)) {
        case 0:
        case 1:
        case 2: {
          const auto value = std::to_string(step);
          const auto reference = expected.try_emplace(key, value);
          require(actual.insert(key, value) == reference.second, "insertion result matches");
          break;
        }
        case 3:
        case 4:
          require(actual.erase(key) == (expected.erase(key) != 0), "erase result matches");
          break;
        case 5:
          actual.reserve(static_cast<std::size_t>(capacity(random)));
          break;
        case 6:
          actual.rehash(static_cast<std::size_t>(capacity(random)));
          break;
        default:
          actual.clear();
          expected.clear();
      }
      check_against(actual, expected);
    }
  });
  return checks.finish();
}
