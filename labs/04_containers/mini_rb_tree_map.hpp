#pragma once
#include <cstddef>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

// 进阶练习：int/string 红黑树。所有空子树使用同一个黑色 NIL 哨兵。
class MiniRBTreeMap {
public:
  enum class Color { red, black };

  // 公开类型仅供只读诊断；调用者不得修改节点或其链接。
  struct Node {
    int key = 0;
    std::string value;
    Color color = Color::black;
    Node* parent = nullptr;
    Node* left = nullptr;
    Node* right = nullptr;
  };

  MiniRBTreeMap() noexcept : root_(&nil_) {
    nil_.parent = nil_.left = nil_.right = &nil_;
  }

  ~MiniRBTreeMap() noexcept {
    clear();
  }

  MiniRBTreeMap(const MiniRBTreeMap&) = delete;
  MiniRBTreeMap& operator=(const MiniRBTreeMap&) = delete;
  MiniRBTreeMap(MiniRBTreeMap&&) = delete;
  MiniRBTreeMap& operator=(MiniRBTreeMap&&) = delete;

  std::size_t size() const noexcept {
    return size_;
  }

  bool empty() const noexcept {
    return size_ == 0;
  }

  const Node* debug_root() const noexcept {
    return root_;
  }

  const Node* debug_nil() const noexcept {
    return &nil_;
  }

  // TODO：重复 key 返回 false 且保留原值；新增后完成红黑性质修复。
  bool insert(int key, const std::string& value) {
    Node* node = root_;
    Node* prev = &nil_;
    bool on_left = false;
    while (node != &nil_) {
      if (key == node->key)
        return false;
      prev = node;
      if (key < node->key) {
        node = node->left;
        on_left = true;
      } else {
        node = node->right;
        on_left = false;
      }
    }
    Node* new_node = new Node(key, value);
    new_node->color = Color::red;
    new_node->left = &nil_;
    new_node->right = &nil_;
    if (prev == &nil_) {
      new_node->color = Color::black;
      new_node->parent = &nil_;
      root_ = new_node;
    } else {
      new_node->color = Color::red;
      new_node->parent = prev;
      if (on_left)
        prev->left = new_node;
      else
        prev->right = new_node;
      if (prev->color == Color::red)
        insert_fixup(new_node);
    }
    size_++;
    return true;
  }

  std::string* find(int key) {
    Node* node = root_;
    while (node != &nil_) {
      if (node->key == key)
        return &node->value;
      else if (node->key < key) {
        node = node->right;
      } else if (node->key > key) {
        node = node->left;
      }
    }
    return nullptr;
  }

  const std::string* find(int key) const {
    Node* node = root_;
    while (node != &nil_) {
      if (node->key == key)
        return &node->value;
      else if (node->key < key) {
        node = node->right;
      } else if (node->key > key) {
        node = node->left;
      }
    }
    return nullptr;
  }

  std::optional<int> lower_bound_key(int key) const {
    if (size_ == 0)
      return std::nullopt;
    Node* node = root_;
    std::optional<int> val;
    while (node != &nil_) {
      if (node->key >= key) {
        val = node->key;
        node = node->left;
      } else {
        node = node->right;
      }
    }
    return val;
  }

  std::vector<std::pair<int, std::string>> items() const {
    std::vector<std::pair<int, std::string>> res;
    res.reserve(size_);
    iterate(root_, res);
    return res;
  }

  // TODO：释放目标节点、维护父子链接，并完成删除后的颜色/黑高修复。
  // 基线约定每次 erase 后重新获取元素指针，不要求 std::map 的删除稳定性。
  bool erase(int key) {
    Node* node = root_;
    while (node != &nil_ && node->key != key)
      node = key < node->key ? node->left : node->right;

    if (node == &nil_)
      return false;

    Node* removed = node;
    Color removed_color = removed->color;
    Node* replacement = &nil_;

    if (node->left == &nil_) {
      replacement = node->right;
      transplant(node, replacement);
    } else if (node->right == &nil_) {
      replacement = node->left;
      transplant(node, replacement);
    } else {
      removed = minimum(node->right);
      removed_color = removed->color;
      replacement = removed->right;
      if (removed->parent == node) {
        replacement->parent = removed;
      } else {
        transplant(removed, replacement);
        removed->right = node->right;
        removed->right->parent = removed;
      }
      transplant(node, removed);
      removed->left = node->left;
      removed->left->parent = removed;
      removed->color = node->color;
    }

    delete node;
    --size_;

    if (removed_color == Color::black)
      erase_fixup(replacement);

    nil_.parent = &nil_;
    return true;
  }

  void clear() noexcept {
    iterate_release(root_);
    root_ = &nil_;
    size_ = 0;
  }

private:
  void rotate_left(Node* node) {
    Node* child = node->right;
    node->right = child->left;
    if (node->right != &nil_)
      node->right->parent = node;
    child->parent = node->parent;
    if (node->parent == &nil_) {
      root_ = child;
    } else if (node == node->parent->left) {
      node->parent->left = child;
    } else {
      node->parent->right = child;
    }
    child->left = node;
    node->parent = child;
  }

  void rotate_right(Node* node) {
    Node* child = node->left;
    node->left = child->right;
    if (node->left != &nil_)
      node->left->parent = node;
    child->parent = node->parent;
    if (node->parent == &nil_) {
      root_ = child;
    } else if (node == node->parent->left) {
      node->parent->left = child;
    } else {
      node->parent->right = child;
    }
    child->right = node;
    node->parent = child;
  }

  void insert_fixup(Node* node) {
    if (node == root_) {
      node->color = Color::black;
      return;
    }
    Node* father = node->parent;
    Node* grand_father = father->parent;
    Node* uncle = father == grand_father->left ? grand_father->right : grand_father->left;
    if (uncle->color == Color::red) {
      father->color = Color::black;
      uncle->color = Color::black;
      grand_father->color = Color::red;
      if (grand_father == root_ || grand_father->parent->color == Color::red)
        insert_fixup(grand_father);
      return;
    }
    if (node == father->right && father == grand_father->left) {
      rotate_left(father);
      rotate_right(grand_father);
      father = node;
    } else if (node == father->left && father == grand_father->right) {
      rotate_right(father);
      rotate_left(grand_father);
      father = node;
    } else if (node == father->left && father == grand_father->left) {
      rotate_right(grand_father);
    } else if (node == father->right && father == grand_father->right) {
      rotate_left(grand_father);
    }
    grand_father->color = Color::red;
    father->color = Color::black;
  }

  void erase_fixup(Node* node) {
    if (node == root_ || node->color == Color::red) {
      node->color = Color::black;
      return;
    }
    Node* father = node->parent;
    Node* brother = father->left == node ? father->right : father->left;
    Node* near_nephew = brother->left;
    Node* far_nephew = brother->right;
    if (brother->color == Color::red && brother == father->right) {
      brother->color = Color::black;
      father->color = Color::red;
      rotate_left(father);
      erase_fixup(node);
    } else if (brother->color == Color::red && brother == father->left) {
      brother->color = Color::black;
      father->color = Color::red;
      rotate_right(father);
      erase_fixup(node);
    } else if (brother->color == Color::black && brother == father->right) {
      if (near_nephew->color == Color::black && far_nephew->color == Color::black) {
        brother->color = Color::red;
        erase_fixup(father);
      } else if (near_nephew->color == Color::red && far_nephew->color == Color::black) {
        near_nephew->color = Color::black;
        brother->color = Color::red;
        rotate_right(brother);
        erase_fixup(node);
      } else if (far_nephew->color == Color::red) {
        brother->color = father->color;
        father->color = Color::black;
        far_nephew->color = Color::black;
        rotate_left(father);
      }
    } else {
      std::swap(near_nephew, far_nephew);
      if (near_nephew->color == Color::black && far_nephew->color == Color::black) {
        brother->color = Color::red;
        erase_fixup(father);
      } else if (near_nephew->color == Color::red && far_nephew->color == Color::black) {
        near_nephew->color = Color::black;
        brother->color = Color::red;
        rotate_left(brother);
        erase_fixup(node);
      } else if (far_nephew->color == Color::red) {
        brother->color = father->color;
        father->color = Color::black;
        far_nephew->color = Color::black;
        rotate_right(father);
      }
    }
  }

  void transplant(Node* old_node, Node* replacement) {
    if (old_node->parent == &nil_) {
      root_ = replacement;
    } else if (old_node == old_node->parent->left) {
      old_node->parent->left = replacement;
    } else {
      old_node->parent->right = replacement;
    }
    replacement->parent = old_node->parent;
  }

  Node* minimum(Node* subtree) const {
    if (!subtree)
      return nullptr;
    Node* node = subtree;
    while (node->left != &nil_) {
      node = node->left;
    }
    return node;
  }

  void iterate(Node* node, std::vector<std::pair<int, std::string>>& res) const {
    if (node == &nil_)
      return;
    iterate(node->left, res);
    res.push_back({node->key, node->value});
    iterate(node->right, res);
  }

  void iterate_release(Node* node) noexcept {
    if (node == &nil_)
      return;
    iterate_release(node->left);
    iterate_release(node->right);
    delete node;
  }

  Node nil_;
  Node* root_;
  std::size_t size_ = 0;
  // 不变量：BST 有序、父子链接一致、根/NIL 为黑、红节点孩子为黑、
  // 从任何节点到后代 NIL 的所有路径黑节点数相同。
};
