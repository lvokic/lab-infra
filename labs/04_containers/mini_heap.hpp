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
    throw std::logic_error("TODO: MiniHeap::top");
  }

  void push(const T& /*value*/) {
    throw std::logic_error("TODO: MiniHeap::push copy");
  }

  void push(T&& /*value*/) {
    throw std::logic_error("TODO: MiniHeap::push move");
  }

  // TODO：移除堆顶；空堆 top/pop 抛 out_of_range。
  void pop() {
    throw std::logic_error("TODO: MiniHeap::pop");
  }

  // TODO：替换全部元素并在线性时间内建堆；不要逐个 push。
  void assign(std::vector<T> /*values*/) {
    throw std::logic_error("TODO: MiniHeap::assign");
  }

  void clear() noexcept {
    elements_.clear();
  }

  // 只读诊断视图，用于检查堆序；修改容器后需重新取得。
  std::span<const T> debug_values() const noexcept {
    return elements_;
  }

private:
  void sift_up(std::size_t /*index*/) {
    throw std::logic_error("TODO: MiniHeap::sift_up");
  }

  void sift_down(std::size_t /*index*/) {
    throw std::logic_error("TODO: MiniHeap::sift_down");
  }

  void heapify() {
    throw std::logic_error("TODO: MiniHeap::heapify");
  }

  std::vector<T> elements_;
  Compare compare_;
  // 不变量：每个有效父子对满足 !compare_(parent, child)。
};
