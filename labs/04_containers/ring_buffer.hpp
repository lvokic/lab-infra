#pragma once
#include <cstddef>
#include <memory>
#include <stdexcept>

// 单线程、固定容量环形缓冲区；满时拒绝新增，不覆盖旧元素。
template <typename T>
class RingBuffer {
public:
  explicit RingBuffer(std::size_t capacity) : capacity_(capacity) {
    if (capacity == 0)
      throw std::invalid_argument("capacity must be positive");
    data_ = allocator_.allocate(capacity);
  }

  ~RingBuffer() noexcept {
    clear();
    allocator_.deallocate(data_, capacity_);
  }

  RingBuffer(const RingBuffer&) = delete;
  RingBuffer& operator=(const RingBuffer&) = delete;
  RingBuffer(RingBuffer&&) = delete;
  RingBuffer& operator=(RingBuffer&&) = delete;

  std::size_t size() const noexcept {
    return size_;
  }

  std::size_t capacity() const noexcept {
    return capacity_;
  }

  bool empty() const noexcept {
    return size_ == 0;
  }

  bool full() const noexcept {
    return size_ == capacity_;
  }

  // TODO：成功构造后返回 true；满时返回 false，且不移动输入对象。
  // 构造抛异常时，已有内容、size 和 head 不变。
  bool try_push(const T& value) {
    if (full())
      return false;
    std::construct_at(data_ + tail_, value);
    tail_ = (tail_ + 1) % capacity_;
    size_++;
    return true;
  }

  bool try_push(T&& value) {
    if (full())
      return false;
    std::construct_at(data_ + tail_, std::move(value));
    tail_ = (tail_ + 1) % capacity_;
    size_++;
    return true;
  }

  // TODO：销毁队首元素，更新位置；空时抛 out_of_range。
  void pop() {
    if (empty())
      throw std::out_of_range("RingBuffer::pop::out of range");
    std::destroy_at(data_ + head_);
    head_ = (head_ + 1) % capacity_;
    size_--;
  }

  T& front() {
    return *(data_ + head_);
  }

  const T& front() const {
    return *(data_ + head_);
  }

  T& back() {
    return *(data_ + tail_);
  }

  const T& back() const {
    return *(data_ + tail_);
  }

  // TODO：index 为逻辑下标，调用方保证 index < size；需要处理物理回绕。
  T& operator[](std::size_t index) {
    size_t offset = (head_ + index) % capacity_;
    return *(data_ + offset);
  }

  const T& operator[](std::size_t index) const {
    size_t offset = (head_ + index) % capacity_;
    return *(data_ + offset);
  }

  void clear() noexcept {
    for (size_t i = 0; i < size_; ++i) {
      size_t offset = (head_ + i) % capacity_;
      std::destroy_at(data_ + offset);
    }
    size_ = 0;
    head_ = 0;
    tail_ = 0;
  }

private:
  std::allocator<T> allocator_;
  T* data_ = nullptr;
  std::size_t capacity_;
  std::size_t head_ = 0;
  std::size_t tail_ = 0;
  std::size_t size_ = 0;
  // 不变量：head < capacity，size <= capacity，只有逻辑 [0,size) 是活对象。
  // 新增不移动已有元素；pop 仅使被移除元素的引用/指针失效。
};
