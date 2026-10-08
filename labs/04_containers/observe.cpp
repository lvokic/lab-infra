#include "lab_check.hpp"
#include <algorithm>
#include <cstddef>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {
template <typename Container>
void show_records(std::string_view label, const Container& records) {
  std::cout << '[' << label << "] size=" << records.size() << " records=[";
  std::string_view separator;
  for (auto it = records.begin(); it != records.end(); ++it) {
    std::cout << separator << it->first << ':' << it->second;
    separator = ", ";
  }
  std::cout << "]\n";
}

void observe_interfaces() {
  std::cout << "\n=== A: interfaces ===\n";
  const std::vector<std::pair<int, std::string>> input{{30, "C"}, {10, "A"}, {20, "B"}};
  auto sequence = input;
  std::map<int, std::string> ordered(input.begin(), input.end());
  std::unordered_map<int, std::string> hashed(input.begin(), input.end());
  show_records("vector", sequence);
  show_records("map", ordered);
  show_records("unordered_map", hashed);

  // vector 没有按 key 查询的成员 find；这里逐个检查 pair 的 first。
  auto sequence_it = std::find_if(sequence.begin(), sequence.end(), [](const auto& record) {
    return record.first == 20;
  });
  auto ordered_it = ordered.find(20);
  auto hashed_it = hashed.find(20);
  require(sequence_it != sequence.end() && ordered_it != ordered.end() && hashed_it != hashed.end(),
          "all three containers find key 20");
  std::cout << "find(20): " << sequence_it->second << ' ' << ordered_it->second << ' '
            << hashed_it->second << '\n';

  sequence.emplace_back(10, "replacement");
  const auto [map_position, map_inserted] = ordered.try_emplace(10, "replacement");
  const auto [hash_position, hash_inserted] = hashed.try_emplace(10, "replacement");
  std::cout << "duplicate key 10: vector size=" << sequence.size()
            << " map inserted=" << map_inserted << " unordered_map inserted=" << hash_inserted
            << '\n';
  require(sequence.size() == 4 && !map_inserted && !hash_inserted && map_position->second == "A" &&
              hash_position->second == "A",
          "vector keeps duplicates; try_emplace preserves the existing value");

  // find 不插入；operator[] 在缺少 key 时插入一个默认构造的 string。
  const auto size_before = ordered.size();
  require(ordered.find(99) == ordered.end() && ordered.size() == size_before,
          "find of an absent key does not insert");
  std::cout << "map find(99): missing, size=" << ordered.size() << '\n';
  auto& default_value = ordered[99];
  std::cout << "map [99]: empty=" << default_value.empty() << " size=" << ordered.size() << '\n';
  require(default_value.empty() && ordered.size() == size_before + 1,
          "operator[] inserts a default value for an absent key");
  ordered.erase(99);

  std::cout << "map keys in [15, 30):";
  auto range_end = ordered.lower_bound(30);
  for (auto it = ordered.lower_bound(15); it != range_end; ++it) {
    std::cout << ' ' << it->first;
  }
  std::cout << '\n';
  require(ordered.lower_bound(15)->first == 20 && range_end->first == 30,
          "lower_bound defines the ordered half-open range");

  auto next = ordered.erase(ordered.find(10));
  require(next != ordered.end() && next->first == 20,
          "map erase(iterator) returns the next iterator");
  std::cout << "map erase(10): next key=" << next->first << '\n';
}

struct ConstantHash {
  std::size_t operator()(int) const noexcept {
    return 0;
  }
};

struct CountingEqual {
  std::size_t* calls;

  bool operator()(int left, int right) const noexcept {
    ++*calls;
    return left == right;
  }
};

template <typename Hash>
void observe_hash(std::string_view label) {
  constexpr int key_count = 64;  // 实验 B：只改这里，依次尝试 16、64、256。
  std::size_t equality_calls = 0;
  std::unordered_map<int, int, Hash, CountingEqual> values(0, Hash{},
                                                           CountingEqual{&equality_calls});
  values.reserve(static_cast<std::size_t>(key_count));
  for (int key = 0; key < key_count; ++key)
    values.try_emplace(key, key * 10);

  const auto report = [&](std::string_view stage) {
    std::size_t occupied = 0;
    std::size_t largest = 0;
    for (std::size_t bucket = 0; bucket < values.bucket_count(); ++bucket) {
      const auto length = values.bucket_size(bucket);
      if (length != 0)
        ++occupied;
      largest = std::max(largest, length);
    }
    equality_calls = 0;  // 不把插入和其他阶段的比较计入本次查询。
    const auto missing = values.find(-1);
    const auto miss_calls = equality_calls;
    require(missing == values.end(), "key -1 is absent");
    std::cout << '[' << label << ' ' << stage << "] size=" << values.size()
              << " buckets=" << values.bucket_count() << " load=" << values.load_factor()
              << " max_load=" << values.max_load_factor() << " occupied=" << occupied
              << " largest_bucket=" << largest << " missing_key_equal_calls=" << miss_calls << '\n';
    if constexpr (std::is_same_v<Hash, ConstantHash>) {
      require(occupied == 1 && largest == values.size(),
              "identical hashes place every key in one bucket");
    }
  };
  report("before rehash");
  values.rehash(values.bucket_count() * 4);
  report("after rehash");
  for (int key = 0; key < key_count; ++key) {
    require(values.at(key) == key * 10, "collisions do not lose distinct keys");
  }
}

void observe_collisions() {
  std::cout << "\n=== B: collisions ===\n";
  observe_hash<std::hash<int>>("default hash");
  observe_hash<ConstantHash>("constant hash");
}

void observe_stability() {
  std::cout << "\n=== C: stability ===\n";
  std::unordered_map<int, std::string> hashed{{10, "A"}, {20, "B"}};
  std::string* value_pointer = nullptr;
  {
    auto old_iterator = hashed.find(10);
    value_pointer = std::addressof(old_iterator->second);
    const auto buckets_before = hashed.bucket_count();
    std::cout << "hash before: buckets=" << buckets_before
              << " value_address=" << static_cast<const void*>(value_pointer) << '\n';
    hashed.rehash(buckets_before * 2 + 1);
    // 发生 rehash 后，old_iterator 不再参与解引用、比较或递增。
  }
  auto fresh_iterator = hashed.find(10);
  require(std::addressof(fresh_iterator->second) == value_pointer && *value_pointer == "A",
          "rehash preserves element pointers and values");
  std::cout << "hash after rehash: buckets=" << hashed.bucket_count()
            << " value_address=" << static_cast<const void*>(value_pointer) << '\n';
  const auto size_before = hashed.size();
  hashed.max_load_factor(0.5F);
  hashed.reserve(100);  // 100 是目标元素数，桶数由负载因子和实现决定。
  fresh_iterator = hashed.find(10);  // reserve 之后重新获取。
  require(hashed.size() == size_before && std::addressof(fresh_iterator->second) == value_pointer,
          "reserve changes bucket capacity without creating elements");
  std::cout << "hash reserve(100): size=" << hashed.size() << " buckets=" << hashed.bucket_count()
            << " max_load=" << hashed.max_load_factor() << '\n';
  // 不再保存迭代器；删除后 value_pointer 也不能继续访问。
  hashed.erase(10);
  value_pointer = nullptr;
  require(!hashed.contains(10), "erasing removes the selected element");

  std::map<int, std::string> ordered{{10, "A"}, {20, "B"}};
  auto kept = ordered.find(10);
  auto* kept_pointer = std::addressof(kept->second);
  ordered.try_emplace(15, "between");
  ordered.erase(20);
  require(kept->first == 10 && std::addressof(kept->second) == kept_pointer,
          "map insertion and erasing another element preserve the kept iterator");
  std::cout << "map after insert(15), erase(20): kept key=" << kept->first << '\n';
}
}  // namespace

int main(int argc, char** argv) {
  std::cout << std::boolalpha;
  const std::string_view experiment = argc == 1 ? "--all" : argv[1];
  if (argc > 2 || (experiment != "--all" && experiment != "--interfaces" &&
                   experiment != "--collisions" && experiment != "--stability")) {
    std::cerr << "usage: container_observe [--all|--interfaces|--collisions|--stability]\n";
    return 2;
  }
  if (experiment == "--all" || experiment == "--interfaces")
    observe_interfaces();
  if (experiment == "--all" || experiment == "--collisions")
    observe_collisions();
  if (experiment == "--all" || experiment == "--stability")
    observe_stability();
  std::cout << "[PASS] " << experiment << '\n';
}
