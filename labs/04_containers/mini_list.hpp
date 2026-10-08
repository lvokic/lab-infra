#pragma once
#include <cstddef>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>

// 双向循环链表；哨兵只存链接，不构造 T。复制/移动与 splice 留到扩展。
template <typename T>
class MiniList {
  struct NodeBase {
    NodeBase* prev = nullptr;
    NodeBase* next = nullptr;
  };

  struct Node : NodeBase {
    T value;

    explicit Node(const T& input) : value(input) {}

    explicit Node(T&& input) : value(std::move(input)) {}
  };

public:
  // 同一套 iterator 外壳提供可修改与只读两个版本。
  template <bool IsConst>
  class BasicIterator {
    using BasePointer = std::conditional_t<IsConst, const NodeBase*, NodeBase*>;
    BasePointer node_ = nullptr;
    friend class MiniList;
    template <bool>
    friend class BasicIterator;

    explicit BasicIterator(BasePointer node) : node_(node) {}

  public:
    using iterator_category = std::bidirectional_iterator_tag;
    using value_type = T;
    using difference_type = std::ptrdiff_t;
    using reference = std::conditional_t<IsConst, const T&, T&>;
    using pointer = std::conditional_t<IsConst, const T*, T*>;
    BasicIterator() = default;

    template <bool Other>
      requires(IsConst && !Other)
    BasicIterator(const BasicIterator<Other>& other) : node_(other.node_) {}

    reference operator*() const {
      throw std::logic_error("TODO: MiniList iterator dereference");
    }

    pointer operator->() const {
      return std::addressof(operator*());
    }

    BasicIterator& operator++() {
      throw std::logic_error("TODO: MiniList iterator ++");
    }

    BasicIterator operator++(int) {
      throw std::logic_error("TODO: MiniList iterator postfix ++");
    }

    BasicIterator& operator--() {
      throw std::logic_error("TODO: MiniList iterator --");
    }

    BasicIterator operator--(int) {
      throw std::logic_error("TODO: MiniList iterator postfix --");
    }

    friend bool operator==(const BasicIterator&, const BasicIterator&) = default;
  };

  using iterator = BasicIterator<false>;
  using const_iterator = BasicIterator<true>;

  MiniList() noexcept : sentinel_{&sentinel_, &sentinel_} {}

  ~MiniList() noexcept {
    clear();
  }

  MiniList(const MiniList&) = delete;
  MiniList& operator=(const MiniList&) = delete;
  MiniList(MiniList&&) = delete;
  MiniList& operator=(MiniList&&) = delete;

  std::size_t size() const noexcept {
    return size_;
  }

  bool empty() const noexcept {
    return size_ == 0;
  }

  iterator begin() {
    throw std::logic_error("TODO: MiniList::begin");
  }

  iterator end() {
    throw std::logic_error("TODO: MiniList::end");
  }

  const_iterator begin() const {
    throw std::logic_error("TODO: MiniList::begin const");
  }

  const_iterator end() const {
    throw std::logic_error("TODO: MiniList::end const");
  }

  T& front() {
    throw std::logic_error("TODO: MiniList::front");
  }

  const T& front() const {
    throw std::logic_error("TODO: MiniList::front const");
  }

  T& back() {
    throw std::logic_error("TODO: MiniList::back");
  }

  const T& back() const {
    throw std::logic_error("TODO: MiniList::back const");
  }

  // TODO：在 position 前构造节点，返回新节点 iterator；position 可为 end。
  // 分配/构造失败时，原有链接、内容和 size 不变。
  iterator insert(const_iterator /*position*/, const T& /*value*/) {
    throw std::logic_error("TODO: MiniList::insert copy");
  }

  iterator insert(const_iterator /*position*/, T&& /*value*/) {
    throw std::logic_error("TODO: MiniList::insert move");
  }

  // TODO：释放指定节点，返回下一个位置；erase(end()) 抛 out_of_range。
  iterator erase(const_iterator /*position*/) {
    throw std::logic_error("TODO: MiniList::erase");
  }

  void push_front(const T& /*value*/) {
    throw std::logic_error("TODO: MiniList::push_front copy");
  }

  void push_front(T&& /*value*/) {
    throw std::logic_error("TODO: MiniList::push_front move");
  }

  void push_back(const T& /*value*/) {
    throw std::logic_error("TODO: MiniList::push_back copy");
  }

  void push_back(T&& /*value*/) {
    throw std::logic_error("TODO: MiniList::push_back move");
  }

  // TODO：空容器的 front/back/pop 都抛 out_of_range。
  void pop_front() {
    throw std::logic_error("TODO: MiniList::pop_front");
  }

  void pop_back() {
    throw std::logic_error("TODO: MiniList::pop_back");
  }

  void clear() noexcept {
    // TODO：只销毁真实节点，恢复哨兵自环，size 归零。
  }

private:
  NodeBase sentinel_;
  std::size_t size_ = 0;
  // 不变量：空时哨兵自环；每条 next/prev 链接互相对应。
  // 插入保持已有元素地址/iterator；删除只使被删节点的句柄失效。
};
