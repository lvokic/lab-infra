// 手写单类型 UniquePtr / SharedPtr / WeakPtr 学习实验。
#include <atomic>
#include <cstddef>
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
  explicit UniquePtr(T *ptr) { ptr_ = ptr; }

public:
  UniquePtr() = default;

  template <typename... Args>
  static UniquePtr<T> make(Args&&... args) {
    return UniquePtr<T>(new T(std::forward<Args>(args)...));
  }

  UniquePtr(const UniquePtr &other) = delete;
  UniquePtr& operator=(const UniquePtr &other) = delete;

  UniquePtr(UniquePtr &&other) noexcept {
    delete ptr_;
    ptr_ = other.ptr_;
    other.ptr_ = nullptr;
  }
  UniquePtr& operator=(UniquePtr &&other) noexcept {
    delete ptr_;
    ptr_ = other.ptr_;
    other.ptr_ = nullptr;
    return *this;
  }

  ~UniquePtr() { delete ptr_; }

  T* get() const noexcept { return ptr_; }

  void reset() noexcept { 
    delete ptr_;
    ptr_ = nullptr;
  }

  T* release() noexcept {
    T *raw = ptr_;
    ptr_ = nullptr;
    return raw;
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

  explicit ControlBlock(U *ptr) noexcept : object(ptr) {}
  void destroy_object() noexcept override {
    delete object;
    object = nullptr;
  }
};


template <typename T>
class WeakPtr;

template <typename T>
class SharedPtr {
private:
  template <typename>
  friend class WeakPtr;

  ControlBlock<T> *cb_ = nullptr;

  struct AdoptStrongRef {};

  explicit SharedPtr(T *ptr) { cb_ = new ControlBlock<T>(ptr); }

  // lock() uses this after it has already incremented strong.
  SharedPtr(ControlBlock<T>* block, AdoptStrongRef) noexcept : cb_(block) {}

  void release() noexcept {
    auto* block = std::exchange(cb_, nullptr);
    if (block == nullptr) return;

    if (block->strong.fetch_sub(1) == 1) {
      block->destroy_object();
      if (block->weak.fetch_sub(1) == 1) {
        delete block;
      }
    }
  }

public:
  SharedPtr() = default;
  ~SharedPtr() { release(); }

  template <typename... Args>
  static SharedPtr<T> make(Args&&... args) {
    T* object = new T(std::forward<Args>(args)...);
    try {
      return SharedPtr<T>(object);
    } catch (...) {
      delete object;
      throw;
    }
  }

  SharedPtr(const SharedPtr& other) noexcept : cb_(other.cb_) {
    if (cb_ != nullptr) cb_->strong.fetch_add(1);
  }

  SharedPtr& operator=(const SharedPtr& other) noexcept {
    if (this == &other) return *this;

    auto* next = other.cb_;
    if (next != nullptr) next->strong.fetch_add(1);
    release();
    cb_ = next;
    return *this;
  }

  SharedPtr(SharedPtr&& other) noexcept
      : cb_(std::exchange(other.cb_, nullptr)) {}

  SharedPtr& operator=(SharedPtr&& other) noexcept {
    if (this == &other) return *this;
    release();
    cb_ = std::exchange(other.cb_, nullptr);
    return *this;
  }

  T* get() const noexcept {
    return cb_ ? cb_->object : nullptr;
  }

  std::size_t use_count() const noexcept {
    return cb_ ? cb_->strong.load() : std::size_t{0};
  }

  explicit operator bool() const noexcept {
    return get() != nullptr;
  }
};


template <typename T>
class WeakPtr {
private:
  ControlBlock<T>* cb_ = nullptr;

  void release() noexcept {
    auto* block = std::exchange(cb_, nullptr);
    if (block != nullptr && block->weak.fetch_sub(1) == 1) {
      delete block;
    }
  }

public:
  WeakPtr() = default;
  ~WeakPtr() { release(); }

  WeakPtr(const SharedPtr<T>& owner) noexcept : cb_(owner.cb_) {
    if (cb_ != nullptr) cb_->weak.fetch_add(1);
  }

  WeakPtr& operator=(const SharedPtr<T>& owner) noexcept {
    auto* next = owner.cb_;
    if (next != nullptr) next->weak.fetch_add(1);
    release();
    cb_ = next;
    return *this;
  }

  WeakPtr(const WeakPtr& other) noexcept : cb_(other.cb_) {
    if (cb_ != nullptr) cb_->weak.fetch_add(1);
  }

  WeakPtr& operator=(const WeakPtr& other) noexcept {
    if (this == &other) return *this;

    auto* next = other.cb_;
    if (next != nullptr) next->weak.fetch_add(1);
    release();
    cb_ = next;
    return *this;
  }

  WeakPtr(WeakPtr&& other) noexcept
      : cb_(std::exchange(other.cb_, nullptr)) {}

  WeakPtr& operator=(WeakPtr&& other) noexcept {
    if (this == &other) return *this;
    release();
    cb_ = std::exchange(other.cb_, nullptr);
    return *this;
  }

  void reset() noexcept { release(); }

  std::size_t use_count() const noexcept {
    return cb_ ? cb_->strong.load() : std::size_t{0};
  }

  bool expired() const noexcept {
    return use_count() == 0;
  }

  SharedPtr<T> lock() const noexcept {
    auto* block = cb_;
    if (block == nullptr) return {};

    auto count = block->strong.load();
    while (count != 0) {
      if (block->strong.compare_exchange_weak(count, count + 1)) {
        return SharedPtr<T>(block, typename SharedPtr<T>::AdoptStrongRef{});
      }
    }
    return {};
  }
};
