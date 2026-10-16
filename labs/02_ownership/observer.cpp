// 手写单类型 UniquePtr / SharedPtr / WeakPtr 学习实验。
#include <atomic>
#include <cstddef>
#include <stdexcept>
#include <utility>

struct Tracked {
  static inline int alive = 0;
  static inline int constructed = 0;
  static inline int destroyed = 0;

  int value = 0;
};

template <typename T>
class UniquePtr {
private:
  T* ptr_ = nullptr;

  explicit UniquePtr(T* ptr) {
    ptr_ = ptr;
  }

public:
  UniquePtr() = default;

  template <typename... Args>
  static UniquePtr<T> make(Args&&... /*args*/) {
    throw std::logic_error("TODO: UniquePtr::make");
  }

  UniquePtr(const UniquePtr& other) = delete;
  UniquePtr& operator=(const UniquePtr& other) = delete;

  UniquePtr(UniquePtr&& /*other*/) noexcept {
    // TODO：移动所有权，并将源指针置空。
  }

  UniquePtr& operator=(UniquePtr&& /*other*/) noexcept {
    // TODO：释放当前对象并接管源对象；处理自移动。
    return *this;
  }

  ~UniquePtr() { /* TODO：删除当前拥有的对象。 */ }

  T* get() const noexcept {
    // TODO：返回被管理对象的借用指针。
    return nullptr;
  }

  void reset() noexcept {
    // TODO：释放当前对象并置空。
  }

  T* release() noexcept {
    // TODO：放弃所有权并返回原指针。
    return nullptr;
  }
};

struct ControlBlockBase {
  std::atomic<std::size_t> strong{1};
  std::atomic<std::size_t> weak{1};

  virtual void destroy_object() noexcept = 0;
  virtual ~ControlBlockBase() = default;
};

template <typename U>
struct ControlBlock final : ControlBlockBase {
  U* object;

  explicit ControlBlock(U* ptr) noexcept : object(ptr) {}

  void destroy_object() noexcept override {
    // TODO：销毁被管理对象。
  }
};

template <typename T>
class WeakPtr;

template <typename T>
class SharedPtr {
private:
  template <typename>
  friend class WeakPtr;

  ControlBlock<T>* cb_ = nullptr;

  struct AdoptStrongRef {};

  explicit SharedPtr(T* /*ptr*/) {
    // TODO：创建控制块并接管对象。
  }

  // lock() uses this after it has already incremented strong.
  SharedPtr(ControlBlock<T>* /*block*/, AdoptStrongRef) noexcept {
    // TODO：接管已经增加强引用计数的控制块。
  }

  void release() noexcept {
    // TODO：递减强引用计数，并在最后一个 owner 离开时清理对象和控制块。
  }

public:
  SharedPtr() = default;

  ~SharedPtr() { /* TODO：释放一个共享 owner。 */ }

  template <typename... Args>
  static SharedPtr<T> make(Args&&... /*args*/) {
    throw std::logic_error("TODO: SharedPtr::make");
  }

  SharedPtr(const SharedPtr& /*other*/) noexcept {
    // TODO：复制共享 owner 并增加强引用计数。
  }

  SharedPtr& operator=(const SharedPtr& /*other*/) noexcept {
    // TODO：处理自赋值、旧 owner 和新 owner。
    return *this;
  }

  SharedPtr(SharedPtr&& /*other*/) noexcept {
    // TODO：移动共享 owner，不改变强引用总数。
  }

  SharedPtr& operator=(SharedPtr&& /*other*/) noexcept {
    // TODO：释放当前 owner 后移动接管；处理自移动。
    return *this;
  }

  T* get() const noexcept {
    // TODO：返回被管理对象的借用指针。
    return nullptr;
  }

  std::size_t use_count() const noexcept {
    // TODO：返回强引用计数。
    return 0;
  }

  explicit operator bool() const noexcept {
    // TODO：判断是否持有对象。
    return false;
  }
};

template <typename T>
class WeakPtr {
private:
  ControlBlock<T>* cb_ = nullptr;

  void release() noexcept {
    // TODO：释放一个弱观察者，并在控制块不再需要时销毁它。
  }

public:
  WeakPtr() = default;

  ~WeakPtr() { /* TODO：释放一个弱观察者。 */ }

  WeakPtr(const SharedPtr<T>& /*owner*/) noexcept {
    // TODO：从共享 owner 建立弱观察，并增加弱引用计数。
  }

  WeakPtr& operator=(const SharedPtr<T>& /*owner*/) noexcept {
    // TODO：替换弱观察关系。
    return *this;
  }

  WeakPtr(const WeakPtr& /*other*/) noexcept {
    // TODO：复制弱观察关系并增加弱引用计数。
  }

  WeakPtr& operator=(const WeakPtr& /*other*/) noexcept {
    // TODO：处理自赋值并替换弱观察关系。
    return *this;
  }

  WeakPtr(WeakPtr&& /*other*/) noexcept {
    // TODO：移动弱观察关系，不改变弱引用总数。
  }

  WeakPtr& operator=(WeakPtr&& /*other*/) noexcept {
    // TODO：移动替换弱观察关系；处理自移动。
    return *this;
  }

  void reset() noexcept {
    // TODO：释放弱观察关系。
  }

  std::size_t use_count() const noexcept {
    // TODO：返回被观察对象的强引用数。
    return 0;
  }

  bool expired() const noexcept {
    // TODO：判断强引用数是否为零。
    return true;
  }

  SharedPtr<T> lock() const noexcept {
    // TODO：原子地尝试增加强引用数；失败时返回空 SharedPtr。
    return {};
  }
};
