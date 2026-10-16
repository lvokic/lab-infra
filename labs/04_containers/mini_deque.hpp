#pragma once
#include <cstddef>
#include <memory>
#include <stdexcept>
#include <vector>

// 简化分块 deque：vector 只存块指针，每块为 BlockSize 个 T 的原始存储。
// 不用 vector<T>/std::deque<T> 保存元素；迭代器和中间插入留作扩展。
template <typename T, std::size_t BlockSize = 4>
class MiniDeque {
  static_assert(BlockSize > 0);

public:
  MiniDeque() noexcept = default;

  ~MiniDeque() noexcept {
    clear();
  }

  MiniDeque(const MiniDeque&) = delete;
  MiniDeque& operator=(const MiniDeque&) = delete;
  MiniDeque(MiniDeque&&) = delete;
  MiniDeque& operator=(MiniDeque&&) = delete;

  std::size_t size() const noexcept {
    return size_;
  }

  bool empty() const noexcept {
    return size_ == 0;
  }

  // TODO：两端构造元素；目录增长不移动已有 T，已有元素地址保持有效。
  // 构造失败时已有元素与 size 不变；已申请的空块可以保留但不得泄漏。
  void push_front(const T& /*value*/) {
    throw std::logic_error("TODO: MiniDeque::push_front copy");
  }

  void push_front(T&& /*value*/) {
    throw std::logic_error("TODO: MiniDeque::push_front move");
  }

  void push_back(const T& /*value*/) {
    throw std::logic_error("TODO: MiniDeque::push_back copy");
  }

  void push_back(T&& /*value*/) {
    throw std::logic_error("TODO: MiniDeque::push_back move");
  }

  // TODO：只销毁被移除元素；空容器的 pop/front/back 抛 out_of_range。
  void pop_front() {
    throw std::logic_error("TODO: MiniDeque::pop_front");
  }

  void pop_back() {
    throw std::logic_error("TODO: MiniDeque::pop_back");
  }

  T& front() {
    throw std::logic_error("TODO: MiniDeque::front");
  }

  const T& front() const {
    throw std::logic_error("TODO: MiniDeque::front const");
  }

  T& back() {
    throw std::logic_error("TODO: MiniDeque::back");
  }

  const T& back() const {
    throw std::logic_error("TODO: MiniDeque::back const");
  }

  // TODO：将逻辑下标转换为块下标与块内偏移；[] 前提 index < size。
  T& operator[](std::size_t /*index*/) {
    throw std::logic_error("TODO: MiniDeque::operator[]");
  }

  const T& operator[](std::size_t /*index*/) const {
    throw std::logic_error("TODO: MiniDeque::operator[] const");
  }

  T& at(std::size_t /*index*/) {
    throw std::logic_error("TODO: MiniDeque::at");
  }

  const T& at(std::size_t /*index*/) const {
    throw std::logic_error("TODO: MiniDeque::at const");
  }

  void clear() noexcept {
    // TODO：销毁活元素、释放所有已分配块、清空目录，位置和 size 归零。
  }

private:
  std::allocator<T> allocator_;
  std::vector<T*> blocks_;
  std::size_t first_block_ = 0;
  std::size_t first_offset_ = 0;
  std::size_t size_ = 0;
  // 不变量：首元素位置由 first_block_/first_offset_ 表示；目录允许空槽。
  // 只有逻辑 [0,size) 是活对象，块内空余槽位不构造 T。
};
