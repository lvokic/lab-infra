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
  void push_front(const T& value) {
    if (empty()) {
      if (first_block_ < blocks_.size() && blocks_[first_block_] != nullptr) {
        std::construct_at(blocks_[first_block_] + first_offset_, value);
      } else {
        T* new_block = allocator_.allocate(BlockSize);
        try {
          std::construct_at(new_block, value);
        } catch (...) {
          allocator_.deallocate(new_block, BlockSize);
          throw;
        }
        try {
          blocks_.push_back(new_block);
        } catch (...) {
          std::destroy_at(new_block);
          allocator_.deallocate(new_block, BlockSize);
          throw;
        }
        first_block_ = blocks_.size() - 1;
        first_offset_ = 0;
      }
      size_++;
      return;
    }
    if (first_offset_ > 0) {
      std::construct_at(blocks_[first_block_] + first_offset_ - 1, value);
      first_offset_ = first_offset_ - 1;
    } else {
      if (first_block_ > 0) {
        if (blocks_[first_block_ - 1] == nullptr) {
          blocks_[first_block_ - 1] = allocator_.allocate(BlockSize);
        }
        std::construct_at(blocks_[first_block_ - 1] + BlockSize - 1, value);
        first_block_ = first_block_ - 1;
        first_offset_ = BlockSize - 1;
      } else {
        T* new_block = allocator_.allocate(BlockSize);
        T* element = new_block + BlockSize - 1;
        try {
          std::construct_at(element, value);
        } catch (...) {
          allocator_.deallocate(new_block, BlockSize);
          throw;
        }
        try {
          blocks_.insert(blocks_.begin(), new_block);
        } catch (...) {
          std::destroy_at(element);
          allocator_.deallocate(new_block, BlockSize);
          throw;
        }
        first_block_ = 0;
        first_offset_ = BlockSize - 1;
      }
    }
    size_++;
  }

  void push_front(T&& value) {
    if (empty()) {
      if (first_block_ < blocks_.size() && blocks_[first_block_] != nullptr) {
        std::construct_at(blocks_[first_block_] + first_offset_, std::move(value));
      } else {
        T* new_block = allocator_.allocate(BlockSize);
        try {
          std::construct_at(new_block, std::move(value));
        } catch (...) {
          allocator_.deallocate(new_block, BlockSize);
          throw;
        }
        try {
          blocks_.push_back(new_block);
        } catch (...) {
          std::destroy_at(new_block);
          allocator_.deallocate(new_block, BlockSize);
          throw;
        }
        first_block_ = blocks_.size() - 1;
        first_offset_ = 0;
      }
      size_++;
      return;
    }
    if (first_offset_ > 0) {
      std::construct_at(blocks_[first_block_] + first_offset_ - 1, std::move(value));
      first_offset_ = first_offset_ - 1;
    } else {
      if (first_block_ > 0) {
        if (blocks_[first_block_ - 1] == nullptr) {
          blocks_[first_block_ - 1] = allocator_.allocate(BlockSize);
        }
        std::construct_at(blocks_[first_block_ - 1] + BlockSize - 1, std::move(value));
        first_block_ = first_block_ - 1;
        first_offset_ = BlockSize - 1;
      } else {
        T* new_block = allocator_.allocate(BlockSize);
        T* element = new_block + BlockSize - 1;
        try {
          std::construct_at(element, std::move(value));
        } catch (...) {
          allocator_.deallocate(new_block, BlockSize);
          throw;
        }
        try {
          blocks_.insert(blocks_.begin(), new_block);
        } catch (...) {
          std::destroy_at(element);
          allocator_.deallocate(new_block, BlockSize);
          throw;
        }
        first_block_ = 0;
        first_offset_ = BlockSize - 1;
      }
    }
    size_++;
  }

  void push_back(const T& value) {
    const std::size_t first_block = empty() ? 0 : first_block_;
    const std::size_t first_offset = empty() ? 0 : first_offset_;
    const std::size_t slot = first_offset + size_;
    const std::size_t block = first_block + slot / BlockSize;
    const std::size_t offset = slot % BlockSize;

    if (block >= blocks_.size())
      blocks_.resize(block + 1, nullptr);
    if (blocks_[block] == nullptr)
      blocks_[block] = allocator_.allocate(BlockSize);

    std::construct_at(blocks_[block] + offset, value);
    first_block_ = first_block;
    first_offset_ = first_offset;
    ++size_;
  }

  void push_back(T&& value) {
    const std::size_t first_block = empty() ? 0 : first_block_;
    const std::size_t first_offset = empty() ? 0 : first_offset_;
    const std::size_t slot = first_offset + size_;
    const std::size_t block = first_block + slot / BlockSize;
    const std::size_t offset = slot % BlockSize;

    if (block >= blocks_.size())
      blocks_.resize(block + 1, nullptr);
    if (blocks_[block] == nullptr)
      blocks_[block] = allocator_.allocate(BlockSize);

    std::construct_at(blocks_[block] + offset, std::move(value));
    first_block_ = first_block;
    first_offset_ = first_offset;
    ++size_;
  }

  // TODO：只销毁被移除元素；空容器的 pop/front/back 抛 out_of_range。
  void pop_front() {
    if (empty())
      throw std::out_of_range("out of range");
    std::destroy_at(blocks_[first_block_] + first_offset_);
    size_--;
    if (size_ == 0) {
      first_block_ = 0;
      first_offset_ = 0;
    } else {
      first_offset_++;
      if (first_offset_ == BlockSize) {
        first_offset_ = 0;
        first_block_++;
      }
    }
  }

  void pop_back() {
    if (empty())
      throw std::out_of_range("out of range");
    const std::size_t slot = first_offset_ + size_ - 1;
    const std::size_t block = first_block_ + slot / BlockSize;
    const std::size_t offset = slot % BlockSize;
    std::destroy_at(blocks_[block] + offset);
    size_--;
    if (size_ == 0) {
      first_block_ = 0;
      first_offset_ = 0;
    }
  }

  T& front() {
    if (empty())
      throw std::out_of_range("out of range");
    return *(blocks_[first_block_] + first_offset_);
  }

  const T& front() const {
    if (empty())
      throw std::out_of_range("out of range");
    return *(blocks_[first_block_] + first_offset_);
  }

  T& back() {
    if (empty())
      throw std::out_of_range("out of range");
    const std::size_t slot = first_offset_ + size_ - 1;
    const std::size_t block = first_block_ + slot / BlockSize;
    const std::size_t offset = slot % BlockSize;
    return *(blocks_[block] + offset);
  }

  const T& back() const {
    if (empty())
      throw std::out_of_range("out of range");
    const std::size_t slot = first_offset_ + size_ - 1;
    const std::size_t block = first_block_ + slot / BlockSize;
    const std::size_t offset = slot % BlockSize;
    return *(blocks_[block] + offset);
  }

  // TODO：将逻辑下标转换为块下标与块内偏移；[] 前提 index < size。
  T& operator[](std::size_t index) {
    if (index >= size())
      throw std::out_of_range("out of range");
    const std::size_t slot = first_offset_ + index;
    const std::size_t block = first_block_ + slot / BlockSize;
    const std::size_t offset = slot % BlockSize;
    return *(blocks_[block] + offset);
  }

  const T& operator[](std::size_t index) const {
    if (index >= size())
      throw std::out_of_range("out of range");
    const std::size_t slot = first_offset_ + index;
    const std::size_t block = first_block_ + slot / BlockSize;
    const std::size_t offset = slot % BlockSize;
    return *(blocks_[block] + offset);
  }

  T& at(std::size_t index) {
    if (index >= size())
      throw std::out_of_range("out of range");
    const std::size_t slot = first_offset_ + index;
    const std::size_t block = first_block_ + slot / BlockSize;
    const std::size_t offset = slot % BlockSize;
    return *(blocks_[block] + offset);
  }

  const T& at(std::size_t index) const {
    if (index >= size())
      throw std::out_of_range("out of range");
    const std::size_t slot = first_offset_ + index;
    const std::size_t block = first_block_ + slot / BlockSize;
    const std::size_t offset = slot % BlockSize;
    return *(blocks_[block] + offset);
  }

  void clear() noexcept {
    while (!empty()) {
      pop_back();
    }
    for (size_t i = 0; i < blocks_.size(); ++i) {
      allocator_.deallocate(blocks_[i], BlockSize);
    }
    blocks_.clear();
    size_ = 0;
    first_block_ = 0;
    first_offset_ = 0;
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
