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
  bool insert(int /*key*/, const std::string& /*value*/) {
    throw std::logic_error("TODO: MiniBSTMap::insert");
  }

  // TODO：找到返回 value 的指针，找不到返回 nullptr，不插入元素。
  std::string* find(int /*key*/) {
    throw std::logic_error("TODO: MiniBSTMap::find");
  }

  const std::string* find(int /*key*/) const {
    throw std::logic_error("TODO: MiniBSTMap::find const");
  }

  // TODO：返回第一个 >= key 的 key；没有则返回 std::nullopt。
  std::optional<int> lower_bound_key(int /*key*/) const {
    throw std::logic_error("TODO: MiniBSTMap::lower_bound_key");
  }

  // TODO：中序遍历，返回按 key 升序排列的快照。
  // 返回 vector 是为了检查结果；树的元素仍存储在独立节点中。
  std::vector<std::pair<int, std::string>> items() const {
    throw std::logic_error("TODO: MiniBSTMap::items");
  }

  // TODO：处理叶节点、单孩子、双孩子和根节点；存在返回 true。
  // 基线约定任何 erase 后都重新获取元素指针，暂不承诺其他节点地址稳定。
  bool erase(int /*key*/) {
    throw std::logic_error("TODO: MiniBSTMap::erase");
  }

  // TODO：释放整棵树，root 置空，size 归零；重复调用安全。
  void clear() noexcept {
    // 析构函数依赖它释放节点；当前仅是空脚手架。
  }

private:
  struct Node {
    int key;
    std::string value;
    Node* left = nullptr;
    Node* right = nullptr;
  };

  Node* root_ = nullptr;
  std::size_t size_ = 0;
  // 不变量：左子树 key < 当前 key < 右子树 key；每个节点唯一拥有。
  // 插入保持原有元素地址；查询/插入/删除的复杂度为 O(h)，最坏 O(n)。
};
