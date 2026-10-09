#pragma once
#include <cstddef>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

// 受限练习：int key / string value，普通二叉搜索树，不做自动平衡。
class MiniBSTMap {
public:
  MiniBSTMap() noexcept = default;

  ~MiniBSTMap() noexcept {
    clear();
  }

  MiniBSTMap(const MiniBSTMap&) = delete;
  MiniBSTMap& operator=(const MiniBSTMap&) = delete;
  MiniBSTMap(MiniBSTMap&&) = delete;
  MiniBSTMap& operator=(MiniBSTMap&&) = delete;

  std::size_t size() const noexcept {
    return size_;
  }

  bool empty() const noexcept {
    return size_ == 0;
  }

  // TODO：新增返回 true；重复 key 返回 false，保留原 value。
  bool insert(int key, const std::string& value) {
    if (empty()) {
      root_ = new Node(key, value);
      size_++;
      return true;
    }
    Node* node = root_;
    Node* prev = nullptr;
    while (node) {
      if (key == node->key)
        return false;
      prev = node;
      if (key < node->key) {
        node = node->left;
      } else {
        node = node->right;
      }
    }
    if (key < prev->key) {
      prev->left = new Node(key, value);
    } else {
      prev->right = new Node(key, value);
    }
    size_++;
    return true;
  }

  // TODO：找到返回 value 的指针，找不到返回 nullptr，不插入元素。
  std::string* find(int key) {
    if (empty())
      return nullptr;
    Node* node = root_;
    while (node) {
      if (key == node->key)
        return &node->value;
      if (key < node->key) {
        node = node->left;
      } else {
        node = node->right;
      }
    }
    return nullptr;
  }

  const std::string* find(int key) const {
    if (empty())
      return nullptr;
    Node* node = root_;
    while (node) {
      if (key == node->key)
        return &node->value;
      if (key < node->key) {
        node = node->left;
      } else {
        node = node->right;
      }
    }
    return nullptr;
  }

  // TODO：返回第一个 >= key 的 key；没有则返回 std::nullopt。
  std::optional<int> lower_bound_key(int key) const {
    if (empty())
      return std::nullopt;
    Node* node = root_;
    Node* cur = nullptr;
    while (node) {
      if (node->key >= key) {
        cur = node;
        node = node->left;
      } else {
        node = node->right;
      }
    }
    return cur == nullptr ? std::nullopt : std::optional<int>(cur->key);
  }

  // TODO：中序遍历，返回按 key 升序排列的快照。
  // 返回 vector 是为了检查结果；树的元素仍存储在独立节点中。
  std::vector<std::pair<int, std::string>> items() const {
    std::vector<std::pair<int, std::string>> res{};
    iterate(res, root_);
    return res;
  }

  // TODO：处理叶节点、单孩子、双孩子和根节点；存在返回 true。
  // 基线约定任何 erase 后都重新获取元素指针，暂不承诺其他节点地址稳定。
  bool erase(int key) {
    if (empty())
      return false;
    std::pair<Node*, Node*> temp = findKey(key);
    Node* parent = temp.first;
    Node* node = temp.second;
    Node* replacemnt = nullptr;
    if (parent == nullptr && node == nullptr)
      return false;
    if (!node->left && !node->right) {
      delete node;
      replacemnt = nullptr;
    } else if (node->left && !node->right) {
      Node* after = node->left;
      node->left = nullptr;
      delete node;
      replacemnt = after;
    } else if (node->right && !node->left) {
      Node* after = node->right;
      node->right = nullptr;
      delete node;
      replacemnt = after;
    } else {
      Node* next = findNextNode(node);
      next->left = node->left;
      next->right = node->right;
      node->left = nullptr;
      node->right = nullptr;
      delete node;
      replacemnt = next;
    }
    if (parent == nullptr) {
      root_ = replacemnt;
    } else if (parent->left == node) {
      parent->left = replacemnt;
    } else {
      parent->right = replacemnt;
    }
    --size_;
    return true;
  }

  // TODO：释放整棵树，root 置空，size 归零；重复调用安全。
  void clear() noexcept {
    iterateRelease(root_);
    root_ = nullptr;
    size_ = 0;
  }

private:
  struct Node {
    int key;
    std::string value;
    Node* left = nullptr;
    Node* right = nullptr;

    explicit Node(int key, const std::string& value) : key(key), value(std::move(value)) {}
  };

  void iterate(std::vector<std::pair<int, std::string>>& res, Node* node) const {
    if (!node)
      return;
    iterate(res, node->left);
    res.push_back({node->key, node->value});
    iterate(res, node->right);
  }

  void iterateRelease(Node* node) {
    if (!node)
      return;
    if (!node->left && !node->right) {
      delete node;
      return;
    }
    iterateRelease(node->left);
    iterateRelease(node->right);
  }

  std::pair<Node*, Node*> findKey(int key) const {
    if (empty())
      return {nullptr, nullptr};
    Node* node = root_;
    Node* parent = nullptr;
    while (node) {
      if (key == node->key)
        return {parent, node};
      parent = node;
      if (key < node->key) {
        node = node->left;
      } else {
        node = node->right;
      }
    }
    return {nullptr, nullptr};
  }

  Node* findNextNode(Node* node) {
    Node* parent = node;
    Node* next = node->right;
    while (next && next->left) {
      parent = next;
      next = next->left;
    }
    Node* after = next->right;
    next->right = nullptr;
    if (parent->left == next) {
      parent->left = after;
    } else {
      parent->right = after;
    }
    return next;
  }

  Node* root_ = nullptr;
  std::size_t size_ = 0;
  // 不变量：左子树 key < 当前 key < 右子树 key；每个节点唯一拥有。
  // 插入保持原有元素地址；查询/插入/删除的复杂度为 O(h)，最坏 O(n)。
};
