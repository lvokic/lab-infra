// 只保留类型名与状态字段；所有函数由你实现。
#include <optional>
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
class SharedPtr {
private:
  ControlBlock<T> *cb_ = nullptr;
  explicit SharedPtr(T *ptr) { cb_ = new ControlBlock<T>(ptr); }

public:
  SharedPtr() = default;
  ~SharedPtr() {
    if (cb_ == nullptr) return;
    if (cb_->strong.fetch_sub(1) == 1) {
      cb_->destroy_object();
      delete cb_;
    }
  }

  template <typename... Args>
  static SharedPtr<T> make(Args&& ...args) {
    return SharedPtr<T>(new T(std::forward<Args>(args)...));
  }

  SharedPtr(const SharedPtr& other) noexcept {
    cb_ = other.cb_;
    cb_->strong.fetch_add(1);
  }
  SharedPtr& operator=(const SharedPtr& other) noexcept {
    cb_->strong.fetch_sub(1);
    if (cb_->strong == 0) {
      cb_->destroy_object();
      delete cb_;
    }
    cb_ = other.cb_;
    cb_->strong.fetch_add(1);
    return *this;
  }

  SharedPtr(SharedPtr &&other) noexcept {
    cb_ = other.cb_;
    other.cb_ = nullptr;
  }
  SharedPtr& operator=(SharedPtr &&other) noexcept {
    cb_->strong.fetch_sub(1);
    if (cb_->strong == 0) {
      cb_->destroy_object();
      delete cb_;
    }
    cb_ = other.cb_;
    other.cb_ = nullptr;
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