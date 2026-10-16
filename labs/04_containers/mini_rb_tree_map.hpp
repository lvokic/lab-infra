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
  bool insert(int /*key*/, const std::string& /*value*/) {
    throw std::logic_error("TODO: MiniRBTreeMap::insert");
  }

  std::string* find(int /*key*/) {
    throw std::logic_error("TODO: MiniRBTreeMap::find");
  }

  const std::string* find(int /*key*/) const {
    throw std::logic_error("TODO: MiniRBTreeMap::find const");
  }

  std::optional<int> lower_bound_key(int /*key*/) const {
    throw std::logic_error("TODO: MiniRBTreeMap::lower_bound_key");
  }

  std::vector<std::pair<int, std::string>> items() const {
    throw std::logic_error("TODO: MiniRBTreeMap::items");
  }

  // TODO：释放目标节点、维护父子链接，并完成删除后的颜色/黑高修复。
  // 基线约定每次 erase 后重新获取元素指针，不要求 std::map 的删除稳定性。
  bool erase(int /*key*/) {
    throw std::logic_error("TODO: MiniRBTreeMap::erase");
  }

  void clear() noexcept {
    // TODO：只释放真实节点，恢复 root=NIL、NIL 黑色及自环、size=0。
  }

private:
  void rotate_left(Node* /*node*/) {
    throw std::logic_error("TODO: RB rotate_left");
  }

  void rotate_right(Node* /*node*/) {
    throw std::logic_error("TODO: RB rotate_right");
  }

  void insert_fixup(Node* /*node*/) {
    throw std::logic_error("TODO: RB insert_fixup");
  }

  void erase_fixup(Node* /*node*/) {
    throw std::logic_error("TODO: RB erase_fixup");
  }

  void transplant(Node* /*old_node*/, Node* /*replacement*/) {
    throw std::logic_error("TODO: RB transplant");
  }

  Node* minimum(Node* /*subtree*/) const {
    throw std::logic_error("TODO: RB minimum");
  }

  Node nil_;
  Node* root_;
  std::size_t size_ = 0;
  // 不变量：BST 有序、父子链接一致、根/NIL 为黑、红节点孩子为黑、
  // 从任何节点到后代 NIL 的所有路径黑节点数相同。
};
