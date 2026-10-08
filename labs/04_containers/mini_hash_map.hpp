#pragma once
#include <cstddef>
#include <functional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

// 受限练习：int key / string value，桶数组 + 单向节点链。
// vector 只管理桶入口数组；不使用标准 map/unordered_map 存储元素。
// Hash 必须可通过 const 对象调用且不抛异常；最大负载因子固定为 1。
template <typename Hash = std::hash<int>>
class MiniHashMap {
  static_assert(std::is_nothrow_invocable_r_v<std::size_t, const Hash&, int>,
                "This exercise requires a noexcept hash function");

public:
  explicit MiniHashMap(std::size_t initial_buckets = 8, Hash hash = {})
      : buckets_(initial_buckets, nullptr), hash_(std::move(hash)) {
    if (initial_buckets == 0)
      throw std::invalid_argument("initial_buckets must be positive");
  }

  ~MiniHashMap() noexcept {
    clear();
  }

  MiniHashMap(const MiniHashMap&) = delete;
  MiniHashMap& operator=(const MiniHashMap&) = delete;
  MiniHashMap(MiniHashMap&&) = delete;
  MiniHashMap& operator=(MiniHashMap&&) = delete;

  std::size_t size() const noexcept {
    return size_;
  }

  bool empty() const noexcept {
    return size_ == 0;
  }

  std::size_t bucket_count() const noexcept {
    return buckets_.size();
  }

  float load_factor() const noexcept {
    return static_cast<float>(size_) / static_cast<float>(bucket_count());
  }

  // TODO：新增 key 返回 true；重复 key 返回 false，保留原 value。
  // 新增后 size <= bucket_count；扩桶时现有元素地址保持有效。
  bool insert(int /*key*/, const std::string& /*value*/) {
    throw std::logic_error("TODO: MiniHashMap::insert");
  }

  // TODO：找到返回 value 的指针，找不到返回 nullptr，不插入元素。
  std::string* find(int /*key*/) {
    throw std::logic_error("TODO: MiniHashMap::find");
  }

  const std::string* find(int /*key*/) const {
    throw std::logic_error("TODO: MiniHashMap::find const");
  }

  // TODO：删除并释放指定节点；存在返回 true，不存在返回 false。
  // 其余元素的指针保持有效。
  bool erase(int /*key*/) {
    throw std::logic_error("TODO: MiniHashMap::erase");
  }

  // TODO：为目标总元素数准备桶；不创建元素，不减少现有桶数。
  void reserve(std::size_t /*total_elements*/) {
    throw std::logic_error("TODO: MiniHashMap::reserve");
  }

  // TODO：桶数至少为 max(requested_buckets, size, 1)。
  // 重新组织现有节点，不复制/移动 value；分配失败时旧状态不变。
  void rehash(std::size_t /*requested_buckets*/) {
    throw std::logic_error("TODO: MiniHashMap::rehash");
  }

  // TODO：销毁全部节点，置空桶入口，size 归零；保留桶数组大小。
  void clear() noexcept {
    // 析构函数依赖它释放节点；当前仅是空脚手架。
  }

  // TODO：统计一个桶的节点数；index 越界抛 std::out_of_range。
  std::size_t bucket_size(std::size_t /*index*/) const {
    throw std::logic_error("TODO: MiniHashMap::bucket_size");
  }

private:
  struct Node {
    int key;
    std::string value;
    Node* next = nullptr;
  };

  std::vector<Node*> buckets_;
  std::size_t size_ = 0;
  Hash hash_;
  // 不变量：每个节点恰好属于一个桶；所有链无环；节点总数等于 size_。
  // 本练习每个非空桶直接指向首节点，与 GCC 的前驱入口布局不同。
};
