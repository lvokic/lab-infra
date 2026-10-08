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
      using NodePointer = std::conditional_t<IsConst, const Node*, Node*>;
      return static_cast<NodePointer>(node_)->value;
    }

    pointer operator->() const {
      return std::addressof(operator*());
    }

    BasicIterator& operator++() {
      node_ = node_->next;
      return *this;
    }

    BasicIterator operator++(int) {
      BasePointer old = node_;
      node_ = node_->next;
      return BasicIterator(old);
    }

    BasicIterator& operator--() {
      node_ = node_->prev;
      return *this;
    }

    BasicIterator operator--(int) {
      BasePointer old = node_;
      node_ = node_->prev;
      return BasicIterator(old);
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
    return iterator(sentinel_.next);
  }

  iterator end() {
    return iterator(&sentinel_);
  }

  const_iterator begin() const {
    return const_iterator(sentinel_.next);
  }

  const_iterator end() const {
    return const_iterator(&sentinel_);
  }

  T& front() {
    if (empty())
      throw std::out_of_range("MiniList:out of range");
    return *begin();
  }

  const T& front() const {
    if (empty())
      throw std::out_of_range("MiniList:out of range");
    return *begin();
  }

  T& back() {
    if (empty())
      throw std::out_of_range("MiniList:out of range");
    auto last = end();
    return *--last;
  }

  const T& back() const {
    if (empty())
      throw std::out_of_range("MiniList:out of range");
    auto last = end();
    return *--last;
  }

  // TODO：在 position 前构造节点，返回新节点 iterator；position 可为 end。
  // 分配/构造失败时，原有链接、内容和 size 不变。
  iterator insert(const_iterator position, const T& value) {
    NodeBase* node = const_cast<NodeBase*>(position.node_);
    NodeBase* before = node->prev;
    Node* new_node = new Node(value);
    before->next = new_node;
    new_node->prev = before;
    node->prev = new_node;
    new_node->next = node;
    size_++;
    return iterator(new_node);
  }

  iterator insert(const_iterator position, T&& value) {
    NodeBase* node = const_cast<NodeBase*>(position.node_);
    NodeBase* before = node->prev;
    Node* new_node = new Node(std::move(value));
    before->next = new_node;
    new_node->prev = before;
    node->prev = new_node;
    new_node->next = node;
    size_++;
    return iterator(new_node);
  }

  // TODO：释放指定节点，返回下一个位置；erase(end()) 抛 out_of_range。
  iterator erase(const_iterator position) {
    if (size() == 0)
      throw std::out_of_range("MiniList:out of range");
    else if (position == end())
      throw std::out_of_range("MiniList:out of range");
    NodeBase* node = const_cast<NodeBase*>(position.node_);
    NodeBase* before = node->prev;
    NodeBase* after = node->next;
    before->next = after;
    after->prev = before;
    node->next = nullptr;
    node->prev = nullptr;
    delete static_cast<Node*>(node);
    size_--;
    return iterator(after);
  }

  void push_front(const T& value) {
    NodeBase* node = const_cast<NodeBase*>(begin().node_);
    Node* new_node = new Node(value);
    sentinel_.next = new_node;
    new_node->prev = &sentinel_;
    new_node->next = node;
    node->prev = new_node;
    size_++;
  }

  void push_front(T&& value) {
    NodeBase* node = const_cast<NodeBase*>(begin().node_);
    Node* new_node = new Node(std::move(value));
    sentinel_.next = new_node;
    new_node->prev = &sentinel_;
    new_node->next = node;
    node->prev = new_node;
    size_++;
  }

  void push_back(const T& value) {
    NodeBase* node = const_cast<NodeBase*>(sentinel_.prev);
    Node* new_node = new Node(value);
    sentinel_.prev = new_node;
    new_node->next = &sentinel_;
    new_node->prev = node;
    node->next = new_node;
    size_++;
  }

  void push_back(T&& value) {
    NodeBase* node = const_cast<NodeBase*>(sentinel_.prev);
    Node* new_node = new Node(std::move(value));
    sentinel_.prev = new_node;
    new_node->next = &sentinel_;
    new_node->prev = node;
    node->next = new_node;
    size_++;
  }

  // TODO：空容器的 front/back/pop 都抛 out_of_range。
  void pop_front() {
    if (empty())
      throw std::out_of_range("MiniList:out of range");
    NodeBase* node = const_cast<NodeBase*>(begin().node_);
    NodeBase* after = node->next;
    after->prev = &sentinel_;
    sentinel_.next = after;
    node->prev = nullptr;
    node->next = nullptr;
    delete static_cast<Node*>(node);
    --size_;
  }

  void pop_back() {
    if (empty())
      throw std::out_of_range("MiniList:out of range");
    NodeBase* node = const_cast<NodeBase*>(sentinel_.prev);
    NodeBase* before = node->prev;
    before->next = &sentinel_;
    sentinel_.prev = before;
    node->prev = nullptr;
    node->next = nullptr;
    delete static_cast<Node*>(node);
    --size_;
  }

  void clear() noexcept {
    while (!empty()) {
      pop_front();
    }
  }

private:
  NodeBase sentinel_;
  std::size_t size_ = 0;
  // 不变量：空时哨兵自环；每条 next/prev 链接互相对应。
  // 插入保持已有元素地址/iterator；删除只使被删节点的句柄失效。
};
