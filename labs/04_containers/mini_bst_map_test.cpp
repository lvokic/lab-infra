#include "check_runner.hpp"
#include "lab_check.hpp"
#include "mini_bst_map.hpp"
#include <map>
#include <random>
#include <type_traits>

namespace {
using Records = std::vector<std::pair<int, std::string>>;

void check_against(const MiniBSTMap& actual, const std::map<int, std::string>& expected) {
  require(actual.size() == expected.size(), "size matches reference");
  require(actual.items() == Records(expected.begin(), expected.end()),
          "in-order snapshot matches ordered reference");
  for (int key = -5; key <= 25; ++key) {
    const auto* found = actual.find(key);
    const auto reference = expected.find(key);
    require((found != nullptr) == (reference != expected.end()), "key presence matches");
    if (found != nullptr)
      require(*found == reference->second, "mapped value matches");
    const auto lower = expected.lower_bound(key);
    const auto expected_key =
        lower == expected.end() ? std::optional<int>{} : std::optional<int>{lower->first};
    require(actual.lower_bound_key(key) == expected_key, "lower_bound matches reference");
  }
}
}  // namespace

int main(int argc, char** argv) {
  static_assert(!std::is_copy_constructible_v<MiniBSTMap>);
  static_assert(
      std::is_same_v<decltype(std::declval<const MiniBSTMap&>().find(0)), const std::string*>);
  ContainerChecks checks(argc, argv);

  checks.run("empty", [] {
    MiniBSTMap values;
    require(values.empty() && values.size() == 0, "initial tree is empty");
  });

  checks.run("insert_find", [] {
    MiniBSTMap values;
    for (int key : {20, 10, 30, 5, 15, 25, 35}) {
      require(values.insert(key, std::to_string(key)), "insert distinct tree keys");
    }
    require(!values.insert(20, "replacement") && values.size() == 7,
            "duplicate key does not replace the value");
    auto* found = values.find(20);
    require(found != nullptr && *found == "20", "find retrieves the original value");
    *found = "updated";
    require(values.insert(40, "40") && values.find(20) == found,
            "insertion preserves existing element addresses");
    const auto& constant = values;
    require(constant.find(20) != nullptr && *constant.find(20) == "updated" &&
                constant.find(99) == nullptr && values.find(99) == nullptr && values.size() == 8,
            "const and non-const lookup work without inserting missing keys");
  });

  checks.run("lower_bound", [] {
    MiniBSTMap values;
    require(!values.lower_bound_key(15), "empty tree has no lower bound");
    for (int key : {20, 10, 30})
      values.insert(key, std::to_string(key));
    require(values.lower_bound_key(0) == 10 && values.lower_bound_key(10) == 10 &&
                values.lower_bound_key(15) == 20 && values.lower_bound_key(30) == 30 &&
                !values.lower_bound_key(31),
            "lower_bound handles exact matches and both ends");
  });

  checks.run("traversal", [] {
    MiniBSTMap values;
    require(values.items().empty(), "empty tree yields an empty snapshot");
    for (int key : {20, 10, 30, 5, 15, 25, 35})
      values.insert(key, std::to_string(key));
    const Records expected{{5, "5"},   {10, "10"}, {15, "15"}, {20, "20"},
                           {25, "25"}, {30, "30"}, {35, "35"}};
    auto snapshot = values.items();
    require(snapshot == expected, "in-order traversal sorts all records by key");
    snapshot.front().second = "snapshot only";
    require(values.find(5) != nullptr && *values.find(5) == "5",
            "snapshot owns copies independently of tree storage");
  });

  checks.run("erase_simple", [] {
    MiniBSTMap values;
    require(!values.erase(99), "erasing from an empty tree returns false");
    for (int key : {20, 10, 30, 5})
      values.insert(key, std::to_string(key));
    require(values.erase(30) && !values.erase(30), "erase leaf and then missing key");
    require(values.erase(10) && values.items() == Records{{5, "5"}, {20, "20"}} &&
                values.size() == 2,
            "erase a node with one child without losing that child");
    require(values.erase(20) && values.items() == Records{{5, "5"}} && values.size() == 1,
            "erase a root with one child");
    require(values.erase(5) && values.empty() && values.items().empty() &&
                values.find(5) == nullptr,
            "erase the last node");
  });

  checks.run("erase_two_children", [] {
    MiniBSTMap values;
    std::map<int, std::string> expected;
    // successor 22 位于右子树内部，并有右孩子 23，检查重新连接是否完整。
    for (int key : {20, 10, 30, 25, 40, 22, 23, 27}) {
      values.insert(key, std::to_string(key));
      expected.try_emplace(key, std::to_string(key));
    }
    for (int key : {20, 30, 10, 25, 22, 40, 23, 27}) {
      require(values.erase(key), "erase an existing key");
      expected.erase(key);
      check_against(values, expected);
    }
    require(values.empty(), "all nodes were removed");
  });

  checks.run("skew_clear_reuse", [] {
    MiniBSTMap values;
    for (int key = 0; key < 128; ++key)
      values.insert(key, std::to_string(key));
    require(values.size() == 128 && values.items().size() == 128 && values.find(127) != nullptr &&
                *values.find(127) == "127" && values.lower_bound_key(127) == 127 &&
                !values.lower_bound_key(128),
            "monotonic insertion remains correct even when the BST degenerates");
    values.clear();
    values.clear();
    require(values.empty() && values.items().empty() && values.find(127) == nullptr &&
                !values.lower_bound_key(0),
            "clear removes every node including a long chain");
    require(values.insert(-1, "reused") && values.find(-1) != nullptr &&
                *values.find(-1) == "reused",
            "cleared tree supports reuse");
  });

  checks.run("random", [] {
    MiniBSTMap actual;
    std::map<int, std::string> expected;
    std::mt19937 random(405);
    std::uniform_int_distribution<int> operation(0, 5);
    std::uniform_int_distribution<int> keys(-5, 25);
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
        default:
          actual.clear();
          expected.clear();
      }
      check_against(actual, expected);
    }
  });
  return checks.finish();
}
