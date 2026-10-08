#pragma once
#include <cstddef>
#include <functional>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

// 二叉堆：vector 管理元素存储，堆序由你维护，不调用标准 heap 算法。
// 与 priority_queue 相同，默认 less 对应最大堆，greater 对应最小堆。
template <typename T, typename Compare = std::less<T>>
class MiniHeap {
public:
  explicit MiniHeap(Compare compare = {}) : compare_(std::move(compare)) {}

  std::size_t size() const noexcept {
    return elements_.size();
  }

  bool empty() const noexcept {
    return elements_.empty();
  }

  const T& top() const {
    if (empty())
      throw std::out_of_range("MiniHeap::out of range");
    return elements_[0];
  }

  void push(const T& value) {
    elements_.push_back(value);
    sift_up(elements_.size() - 1);
  }

  void push(T&& value) {
    elements_.push_back(std::move(value));
    sift_up(elements_.size() - 1);
  }

  // 移除堆顶；空堆 top/pop 抛 out_of_range。
  void pop() {
    if (empty())
      throw std::out_of_range("MiniHeap::out of range");
    if (size() > 1)
      elements_.front() = std::move(elements_.back());
    elements_.pop_back();
    if (!empty())
      sift_down(0);
  }

  // TODO：替换全部元素并在线性时间内建堆；不要逐个 push。
  void assign(std::vector<T> values) {
    clear();
    elements_ = std::move(values);
    heapify();
  }

  void clear() noexcept {
    elements_.clear();
  }

  // 只读诊断视图，用于检查堆序；修改容器后需重新取得。
  std::span<const T> debug_values() const noexcept {
    return elements_;
  }

private:
  void sift_up(std::size_t index) {
    size_t n = size();
    if (index >= n)
      throw std::out_of_range("MiniHeap::out of range");
    while (index > 0) {
      size_t parent = (index - 1) / 2;
      if (!compare_(elements_[parent], elements_[index]))
        return;
      std::swap(elements_[parent], elements_[index]);
      index = parent;
    }
  }

  void sift_down(std::size_t index) {
    const std::size_t n = size();
    // index < n / 2 的节点才有左孩子；叶子节点不需要下沉。
    while (index < n / 2) {
      const std::size_t left = index * 2 + 1;
      const std::size_t right = left + 1;
      std::size_t tmp = left;
      if (right < n && compare_(elements_[left], elements_[right]))
        tmp = right;
      if (!compare_(elements_[index], elements_[tmp]))
        return;
      std::swap(elements_[index], elements_[tmp]);
      index = tmp;
    }
  }

  void heapify() {
    const std::size_t n = size();
    for (std::size_t i = n / 2; i > 0; --i) {
      sift_down(i - 1);
    }
  }

  std::vector<T> elements_;
  Compare compare_;
  // 不变量：每个有效父子对满足 !compare_(parent, child)。
};
