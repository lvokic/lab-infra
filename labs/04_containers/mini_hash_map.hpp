#pragma once
#include <algorithm>
#include <cstddef>
#include <functional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <cstring>
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
  bool insert(int key, const std::string& value) {
    if (size_ >= bucket_count()) {
      rehash(bucket_count() * 2 + 1);
      return insert(key, value);
    }
    size_t index = hash_(key) % bucket_count();
    Node* node = buckets_[index];
    if (node == nullptr) {
      node = new Node(key, value);
      buckets_[index] = node;
      size_++;
      return true;
    }
    Node* prev = nullptr;
    while (node) {
      if (key == node->key)
        return false;
      prev = node;
      node = node->next;
    }
    prev->next = new Node(key, value);
    size_++;
    return true;
  }

  // TODO：找到返回 value 的指针，找不到返回 nullptr，不插入元素。
  std::string* find(int key) {
    if (empty())
      return nullptr;
    size_t index = hash_(key) % bucket_count();
    Node* node = buckets_[index];
    if (node == nullptr) {
      return nullptr;
    }
    while (node) {
      if (key == node->key)
        return &node->value;
      node = node->next;
    }
    return nullptr;
  }

  const std::string* find(int key) const {
    if (empty())
      return nullptr;
    size_t index = hash_(key) % bucket_count();
    Node* node = buckets_[index];
    if (node == nullptr) {
      return nullptr;
    }
    while (node) {
      if (key == node->key)
        return &node->value;
      node = node->next;
    }
    return nullptr;
  }

  // TODO：删除并释放指定节点；存在返回 true，不存在返回 false。
  // 其余元素的指针保持有效。
  bool erase(int key) {
    if (empty())
      return false;
    size_t index = hash_(key) % bucket_count();
    Node* node = buckets_[index];
    if (node == nullptr) {
      return false;
    }
    if (node->key == key) {
      buckets_[index] = node->next;
      node->next = nullptr;
      delete node;
      size_--;
      return true;
    }
    Node* prev = nullptr;
    while (node) {
      if (key == node->key) {
        prev->next = node->next;
        node->next = nullptr;
        delete node;
        size_--;
        return true;
      }
      prev = node;
      node = node->next;
    }
    return false;
  }

  // TODO：为目标总元素数准备桶；不创建元素，不减少现有桶数。
  void reserve(std::size_t total_elements) {
    if (total_elements > bucket_count()) {
      rehash(total_elements);
    }
  }

  // TODO：桶数至少为 max(requested_buckets, size, 1)。
  // 重新组织现有节点，不复制/移动 value；分配失败时旧状态不变。
  void rehash(std::size_t requested_buckets) {
    const std::size_t count = std::max({requested_buckets, size(), std::size_t{1}});
    std::vector<Node*> old_buckets = buckets_;
    std::vector<Node*> new_buckets(count, nullptr);
    buckets_ = std::move(new_buckets);
    for (size_t i = 0; i < old_buckets.size(); ++i) {
      while (old_buckets[i]) {
        size_t index = hash_(old_buckets[i]->key) % count;
        Node* after = old_buckets[i]->next;
        old_buckets[i]->next = nullptr;
        if (buckets_[index] == nullptr) {
          buckets_[index] = old_buckets[i];
        } else {
          Node* tail = findTail(index);
          tail->next = old_buckets[i];
        }
        old_buckets[i] = after;
      }
    }
    old_buckets.clear();
  }

  // TODO：销毁全部节点，置空桶入口，size 归零；保留桶数组大小。
  void clear() noexcept {
    if (empty())
      return;
    for (size_t i = 0; i < buckets_.size(); ++i) {
      while (buckets_[i]) {
        Node* after = buckets_[i]->next;
        buckets_[i]->next = nullptr;
        delete buckets_[i];
        buckets_[i] = after;
      }
    }
    size_ = 0;
  }

  // TODO：统计一个桶的节点数；index 越界抛 std::out_of_range。
  std::size_t bucket_size(std::size_t index) const {
    if (index >= bucket_count()) {
      throw std::out_of_range("MiniHashMap::bucket_size");
    }
    size_t count = 0;
    Node* node = buckets_[index];
    while (node) {
      count++;
      node = node->next;
    }
    return count;
  }

private:
  struct Node {
    int key;
    std::string value;
    Node* next = nullptr;

    explicit Node(int key, const std::string& value) : key(key), value(std::move(value)) {}
  };

  Node* findTail(size_t index) {
    Node* node = buckets_[index];
    Node* prev = nullptr;
    while (node) {
      prev = node;
      node = node->next;
    }
    return prev;
  }

  std::vector<Node*> buckets_;
  std::size_t size_ = 0;
  Hash hash_;
  // 不变量：每个节点恰好属于一个桶；所有链无环；节点总数等于 size_。
  // 本练习每个非空桶直接指向首节点，与 GCC 的前驱入口布局不同。
};
