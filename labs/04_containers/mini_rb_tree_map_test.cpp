#include "check_runner.hpp"
#include "exercise_test_helpers.hpp"
#include "mini_rb_tree_map.hpp"
#include <algorithm>
#include <limits>
#include <map>
#include <random>
#include <unordered_set>

namespace {
using Tree = MiniRBTreeMap;
using Records = std::vector<std::pair<int, std::string>>;

// 独立检查结构，不调用学习者编写的“验证成功”函数。
// 这是性质检查，不包含插入、删除、旋转或修复算法。
int check_subtree(const Tree::Node* node, const Tree::Node* nil, const Tree::Node* parent,
                  long long lower, long long upper, std::unordered_set<const Tree::Node*>& visited,
                  Records& records) {
  require(node != nullptr, "all child links use NIL instead of nullptr");
  if (node == nil)
    return 1;
  require(visited.insert(node).second, "tree has no cycle or shared node");
  require(node->parent == parent, "parent links agree with child links");
  require(lower < node->key && node->key < upper, "BST ordering holds throughout each subtree");
  require(node->left != nullptr && node->right != nullptr, "real nodes have valid child links");
  require(node->color == Tree::Color::red || node->color == Tree::Color::black, "valid node color");
  if (node->color == Tree::Color::red) {
    require(node->left->color == Tree::Color::black && node->right->color == Tree::Color::black,
            "red nodes have black children");
  }
  const int left_height = check_subtree(node->left, nil, node, lower, node->key, visited, records);
  records.emplace_back(node->key, node->value);
  const int right_height =
      check_subtree(node->right, nil, node, node->key, upper, visited, records);
  require(left_height == right_height, "left and right paths have equal black height");
  return left_height + (node->color == Tree::Color::black ? 1 : 0);
}

void check_against(const Tree& actual, const std::map<int, std::string>& expected) {
  const auto* nil = actual.debug_nil();
  const auto* root = actual.debug_root();
  require(nil != nullptr && root != nullptr && nil->color == Tree::Color::black &&
              nil->left == nil && nil->right == nil,
          "NIL is a black leaf sentinel");
  if (root != nil)
    require(root->parent == nil && root->color == Tree::Color::black,
            "root is black with NIL parent");
  std::unordered_set<const Tree::Node*> visited;
  Records records;
  check_subtree(root, nil, nil, std::numeric_limits<long long>::lowest(),
                std::numeric_limits<long long>::max(), visited, records);
  require(visited.size() == actual.size() && actual.size() == expected.size(),
          "reachable node count");
  const Records reference(expected.begin(), expected.end());
  require(records == reference && actual.items() == reference, "tree storage and traversal match");
  for (const auto& [key, value] : expected) {
    const auto* found = actual.find(key);
    require(found != nullptr && *found == value, "every reference key is findable");
  }
}
}  // namespace

int main(int argc, char** argv) {
  ContainerChecks checks(argc, argv);
  checks.run("empty", [] {
    Tree values;
    const auto* nil = values.debug_nil();
    require(values.empty() && values.size() == 0 && values.debug_root() == nil &&
                nil->color == Tree::Color::black && nil->left == nil && nil->right == nil,
            "initial tree uses a black NIL sentinel");
  });
  checks.run("insert_find", [] {
    Tree values;
    std::map<int, std::string> expected;
    for (int key : {20, 10, 30, 5, 15}) {
      const auto value = std::to_string(key);
      require(values.insert(key, value), "insert new key");
      expected.try_emplace(key, value);
      check_against(values, expected);
    }
    require(!values.insert(20, "replacement") && values.find(99) == nullptr,
            "duplicates and missing lookup do not change the tree");
    auto* found = values.find(20);
    require(found != nullptr, "mutable lookup finds value");
    *found = "updated";
    expected[20] = "updated";
    require(values.insert(25, "25") && values.find(20) == found,
            "insertion preserves value addresses");
    expected.try_emplace(25, "25");
    check_against(values, expected);
  });
  checks.run("lower_traversal", [] {
    Tree values;
    require(!values.lower_bound_key(0) && values.items().empty(), "empty queries");
    for (int key : {20, 10, 30})
      values.insert(key, std::to_string(key));
    require(values.lower_bound_key(0) == 10 && values.lower_bound_key(10) == 10 &&
                values.lower_bound_key(15) == 20 && values.lower_bound_key(30) == 30 &&
                !values.lower_bound_key(31),
            "lower_bound at exact keys and boundaries");
    auto snapshot = values.items();
    require(snapshot == Records{{10, "10"}, {20, "20"}, {30, "30"}}, "ordered snapshot");
    snapshot.front().second = "independent";
    require(values.find(10) != nullptr && *values.find(10) == "10", "snapshot is independent");
  });
  checks.run("rotations", [] {
    for (const auto& keys : std::vector<std::vector<int>>{{30, 20, 10},
                                                          {10, 20, 30},
                                                          {30, 10, 20},
                                                          {10, 30, 20},
                                                          {10, 5, 15, 1, 6, 12, 18, 0}}) {
      Tree values;
      std::map<int, std::string> expected;
      for (int key : keys) {
        values.insert(key, std::to_string(key));
        expected.try_emplace(key, std::to_string(key));
        check_against(values, expected);
      }
    }
  });
  checks.run("sorted_growth", [] {
    for (bool reverse : {false, true}) {
      Tree values;
      std::map<int, std::string> expected;
      for (int step = 0; step < 256; ++step) {
        const int key = reverse ? 255 - step : step;
        values.insert(key, std::to_string(key));
        expected.try_emplace(key, std::to_string(key));
        check_against(values, expected);
      }
    }
  });
  checks.run("erase", [] {
    for (int order = 0; order < 3; ++order) {
      Tree values;
      std::map<int, std::string> expected;
      std::vector<int> keys;
      require(!values.erase(99), "erase from empty tree");
      for (int key = 0; key < 31; ++key) {
        keys.push_back(key);
        values.insert(key, std::to_string(key));
        expected.try_emplace(key, std::to_string(key));
      }
      if (order == 1)
        std::reverse(keys.begin(), keys.end());
      if (order == 2) {
        std::mt19937 random(410);
        std::shuffle(keys.begin(), keys.end(), random);
      }
      for (int key : keys) {
        require(values.erase(key) && !values.erase(key), "erase exactly once");
        expected.erase(key);
        check_against(values, expected);
      }
    }
  });
  checks.run("clear_reuse", [] {
    Tree values;
    for (int key = 0; key < 20; ++key)
      values.insert(key, std::to_string(key));
    values.clear();
    values.clear();
    check_against(values, {});
    require(values.insert(-1, "reused"), "reuse after clear");
    check_against(values, {{-1, "reused"}});
  });
  checks.run("random", [] {
    Tree actual;
    std::map<int, std::string> expected;
    std::mt19937 random(411);
    std::uniform_int_distribution<int> operation(0, 9);
    std::uniform_int_distribution<int> keys(-40, 40);
    for (int step = 0; step < 800; ++step) {
      const int key = keys(random);
      const int op = operation(random);
      if (op < 5) {
        const auto value = std::to_string(step);
        const auto inserted = expected.try_emplace(key, value).second;
        require(actual.insert(key, value) == inserted, "insertion result matches");
      } else if (op < 9) {
        require(actual.erase(key) == (expected.erase(key) != 0), "erase result matches");
      } else {
        actual.clear();
        expected.clear();
      }
      check_against(actual, expected);
      const auto reference = expected.lower_bound(key);
      const auto lower =
          reference == expected.end() ? std::optional<int>{} : std::optional<int>{reference->first};
      require(actual.lower_bound_key(key) == lower, "random lower_bound matches");
      const auto* found = actual.find(key);
      require((found != nullptr) == expected.contains(key), "random key presence matches");
    }
  });
  return checks.finish();
}
