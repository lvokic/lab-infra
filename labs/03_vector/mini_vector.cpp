#include "lab_check.hpp"
#include <cstddef>
#include <exception>
#include <iostream>
#include <memory>
#include <random>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

template <typename T>
class MiniVector {
public:
  MiniVector() noexcept = default;

  ~MiniVector() noexcept { releaseStorage(); }

  MiniVector(const MiniVector&) = delete;
  MiniVector& operator=(const MiniVector&) = delete;

  // TODO：转交资源；本练习约定普通移动后源为空。
  MiniVector(MiniVector&& other) noexcept {
    allocator_ = other.allocator_;
    data_ = other.data_;
    size_ = other.size_;
    capacity_ = other.capacity_;
    other.data_ = nullptr;
    other.size_ = 0;
    other.capacity_ = 0;
  }

  // TODO：替换已有资源；自移动保留自身内容；返回当前对象的引用。
  MiniVector& operator=(MiniVector&& other) noexcept {
    if (this == &other) return *this;
    releaseStorage();
    allocator_ = other.allocator_;
    data_ = other.data_;
    size_ = other.size_;
    capacity_ = other.capacity_;
    other.data_ = nullptr;
    other.size_ = 0;
    other.capacity_ = 0;
    return *this;
  }

  // TODO：只查询状态，不改变元素或存储。
  std::size_t size() const noexcept { return size_; }
  std::size_t capacity() const noexcept { return capacity_; }

  // TODO：容量至少为 n；size 与已有元素内容不变。
  // n 不超过当前容量时状态不变；支持的类型范围内提供强异常保证。
  void reserve(std::size_t n) {
    if (n <= capacity_) return;
    T* new_data = allocator_.allocate(n);
    std::size_t constructed = 0;
    try {
      for (; constructed < size_; ++constructed) {
        std::construct_at(new_data + constructed,
                          std::move_if_noexcept(data_[constructed]));
      }
    } catch (...) {
      // 失败的槽位没有活对象，只销毁已经成功构造的前缀。
      std::destroy_n(new_data, constructed);
      allocator_.deallocate(new_data, n);
      throw;
    }
    const auto old_size = size_;
    releaseStorage();
    capacity_ = n;
    data_ = new_data;
    size_ = old_size;
  }

  // 基线约定：追加参数不得引用本容器中的元素。
  // 新元素构造失败时大小和原元素不变，但 reserve 可能已经增大容量。
  void push_back(const T& value) {
    if (size_ == capacity_) {
      reserve(capacity_ == 0 ? 1 : capacity_ * 2);
    }
    std::construct_at(data_ + size_, value);
    ++size_;
  }

  void push_back(T&& value) {
    if (size_ == capacity_) {
      reserve(capacity_ == 0 ? 1 : capacity_ * 2);
    }
    std::construct_at(data_ + size_, std::move(value));
    ++size_;
  }

  // TODO：结束所有活元素的生命周期，保留容量。
  void clear() noexcept {
    for (size_t i = 0; i < size_; ++i) {
      std::destroy_at(data_ + i);
    }
    size_ = 0;
  }

  // TODO：访问已有元素；调用方保证 index < size，不进行自动扩容。
  T& operator[](std::size_t index) noexcept { return *(data_ + index); }
  const T& operator[](std::size_t index) const noexcept { return *(data_ + index); }

private:
  void releaseStorage() noexcept {
    clear();
    if (data_ != nullptr) {
      allocator_.deallocate(data_, capacity_);
    }
    data_ = nullptr;
    capacity_ = 0;
  }

  // 分配器管理存储；本练习不实现自定义 allocator 传播。
  std::allocator<T> allocator_;
  T* data_ = nullptr;
  std::size_t size_ = 0;
  std::size_t capacity_ = 0;

  // 不变量：0 <= size_ <= capacity_；只有 [0, size_) 是活元素。
};

int main() {
  static_assert(!std::is_copy_constructible_v<MiniVector<int>>);
  static_assert(!std::is_copy_assignable_v<MiniVector<int>>);
  static_assert(std::is_nothrow_move_constructible_v<MiniVector<int>>);
  static_assert(std::is_nothrow_move_assignable_v<MiniVector<int>>);

  int passed = 0;
  int failed = 0;
  auto run = [&](std::string_view name, auto check) {
    // 先刷新名称；即使实现崩溃，也能定位正在执行的检查。
    std::cout << "[RUN] " << name << std::endl;
    try {
      check();
      ++passed;
      std::cout << "[PASS] " << name << '\n';
    } catch (const std::exception& error) {
      ++failed;
      std::cerr << "[FAIL] " << name << ": " << error.what() << '\n';
    } catch (...) {
      ++failed;
      std::cerr << "[FAIL] " << name << ": unexpected exception\n";
    }
  };

  run("empty state and repeated clear", [] {
    MiniVector<int> values;
    require(values.size() == 0 && values.capacity() == 0, "default state is empty");
    values.reserve(0);
    values.clear();
    values.clear();
    require(values.size() == 0 && values.capacity() == 0,
            "reserve(0) and clearing an empty vector preserve its state");
  });

  run("reserved storage, copy/move append and const access", [] {
    MiniVector<int> values;
    values.reserve(2);
    require(values.size() == 0 && values.capacity() >= 2,
            "reserve creates capacity without elements");
    const int first = 10;
    values.push_back(first);  // const T& 重载。
    values.push_back(20);     // T&& 重载。
    require(values.size() == 2 && values[0] == 10 && values[1] == 20,
            "both append overloads preserve order and values");
    values[0] = 11;
    const auto& view = values;
    require(view[0] == 11 && view[1] == 20, "const access sees element updates");
  });

  run("reserve no-op and relocation preserve content", [] {
    MiniVector<int> values;
    values.reserve(2);
    values.push_back(10);
    values.push_back(20);
    const auto capacity = values.capacity();
    values.reserve(0);
    values.reserve(capacity);
    require(values.capacity() == capacity && values.size() == 2,
            "reserve within capacity neither shrinks nor changes size");
    values.reserve(capacity + 3);
    require(values.capacity() >= capacity + 3 && values.size() == 2 &&
            values[0] == 10 && values[1] == 20,
            "reserve beyond capacity preserves every existing element");
  });

  run("append from zero capacity and repeated growth", [] {
    MiniVector<int> values;
    for (int i = 0; i < 64; ++i) {
      const int value = i * 3;
      if (i % 2 == 0) values.push_back(value);
      else values.push_back(int(value));
      require(values.size() == static_cast<std::size_t>(i + 1) &&
              values.size() <= values.capacity(), "growth preserves size <= capacity");
      for (int j = 0; j <= i; ++j) {
        require(values[static_cast<std::size_t>(j)] == j * 3,
                "growth preserves all previously appended values");
      }
    }
  });

  run("clear retains capacity and allows reuse", [] {
    MiniVector<int> values;
    values.reserve(3);
    values.push_back(10);
    values.push_back(20);
    const auto capacity = values.capacity();
    values.clear();
    values.clear();
    require(values.size() == 0 && values.capacity() == capacity,
            "repeated clear retains the allocated capacity");
    values.push_back(30);
    require(values.size() == 1 && values[0] == 30 && values.capacity() == capacity,
            "append after clear reuses capacity");
  });

  run("move construction and reuse of the source", [] {
    MiniVector<int> source;
    source.reserve(3);
    source.push_back(10);
    source.push_back(20);
    const auto capacity = source.capacity();
    MiniVector<int> destination(std::move(source));
    require(source.size() == 0 && source.capacity() == 0, "move leaves source empty");
    require(destination.size() == 2 && destination.capacity() == capacity &&
            destination[0] == 10 && destination[1] == 20, "move transfers content and capacity");
    source.push_back(99);
    require(source.size() == 1 && source[0] == 99 && destination[0] == 10,
            "moved-from source can be reused independently");
  });

  run("move assignment, self-move and assignment from empty", [] {
    MiniVector<int> source;
    source.reserve(2);
    source.push_back(10);
    source.push_back(20);
    MiniVector<int> destination;
    destination.reserve(1);
    destination.push_back(99);
    auto& result = (destination = std::move(source));
    require(&result == &destination, "move assignment returns destination by reference");
    require(source.size() == 0 && source.capacity() == 0 && destination.size() == 2 &&
            destination[0] == 10 && destination[1] == 20, "move replaces the previous content");
    const auto capacity = destination.capacity();
    auto* alias = &destination;
    destination = std::move(*alias);
    require(destination.size() == 2 && destination.capacity() == capacity &&
            destination[0] == 10 && destination[1] == 20, "self-move preserves content by contract");
    MiniVector<int> empty;
    destination = std::move(empty);
    require(destination.size() == 0 && destination.capacity() == 0,
            "assignment from empty releases the destination's resources");
  });

  // 本地测试类型：只记录元素生命周期，不实现容器的任何资源管理。
  struct Counts {
    int alive = 0;
    int constructed = 0;
    int destroyed = 0;
  };
  struct Probe {
    Counts* counts;
    int value;
    Probe(Counts& counters, int input) : counts(&counters), value(input) {
      ++counts->alive;
      ++counts->constructed;
    }
    Probe(const Probe& other) : counts(other.counts), value(other.value) {
      ++counts->alive;
      ++counts->constructed;
    }
    Probe(Probe&& other) noexcept : counts(other.counts), value(other.value) {
      other.value = -1;
      ++counts->alive;
      ++counts->constructed;
    }
    ~Probe() {
      --counts->alive;
      ++counts->destroyed;
    }
  };

  run("element lifetimes during reserve, clear and destruction", [] {
    Counts counts;
    {
      MiniVector<Probe> values;
      values.reserve(2);
      require(counts.alive == 0 && counts.constructed == 0,
              "reserving empty storage constructs no Probe objects");
      {
        const Probe seed(counts, 10);
        values.push_back(seed);
        require(counts.alive == 2, "copy append creates one separate element");
      }
      values.push_back(Probe(counts, 20));
      require(counts.alive == 2, "only the two container elements remain alive");
      values.reserve(values.capacity() + 1);
      require(values.size() == 2 && values[0].value == 10 && values[1].value == 20 &&
              counts.alive == 2, "relocation cleans up old objects and preserves values");
      values.clear();
      require(counts.alive == 0 && counts.constructed == counts.destroyed,
              "clear destroys every live element");
      values.push_back(Probe(counts, 30));
      require(counts.alive == 1, "reuse constructs one new live element");
    }
    require(counts.alive == 0 && counts.constructed == counts.destroyed,
            "destruction leaves no live Probe objects");
  });

  run("move assignment destroys replaced elements", [] {
    Counts counts;
    {
      MiniVector<Probe> source;
      source.reserve(1);
      source.push_back(Probe(counts, 10));
      MiniVector<Probe> destination;
      destination.reserve(1);
      destination.push_back(Probe(counts, 99));
      const auto constructed = counts.constructed;
      destination = std::move(source);
      require(counts.alive == 1 && counts.constructed == constructed &&
              destination.size() == 1 && destination[0].value == 10,
              "container move transfers ownership and destroys the replaced element");
    }
    require(counts.alive == 0 && counts.constructed == counts.destroyed,
            "both moved containers can be destroyed without duplicate cleanup");
  });

  run("noexcept move-only elements survive growth", [] {
    MiniVector<std::unique_ptr<int>> values;
    values.reserve(1);
    values.push_back(std::make_unique<int>(10));
    values.push_back(std::make_unique<int>(20));
    require(values.size() == 2 && values[0] && values[1] &&
            *values[0] == 10 && *values[1] == 20, "growth supports move-only elements");
    values.reserve(values.capacity() + 2);
    require(values[0] && values[1] && *values[0] == 10 && *values[1] == 20,
            "reserve retains ownership of move-only resources");
  });

  struct CopyFailure : std::exception {};
  struct ThrowingProbe {
    int* alive;
    int* copies_left;
    int value;
    ThrowingProbe(int& live_count, int& budget, int input)
        : alive(&live_count), copies_left(&budget), value(input) { ++*alive; }
    ThrowingProbe(const ThrowingProbe& other)
        : alive(other.alive), copies_left(other.copies_left), value(other.value) {
      if (*copies_left == 0) throw CopyFailure{};
      if (*copies_left > 0) --*copies_left;
      ++*alive;  // 构造成功后才计入活对象。
    }
    ThrowingProbe(ThrowingProbe&& other) noexcept(false)
        : alive(other.alive), copies_left(other.copies_left), value(other.value) {
      throw CopyFailure{};  // reserve 对此可复制类型应选择拷贝。
    }
    ~ThrowingProbe() { --*alive; }
  };

  run("reserve rolls back after a partial copy failure", [] {
    int alive = 0;
    int copies_left = -1;  // 负数表示暂不注入复制失败。
    {
      MiniVector<ThrowingProbe> values;
      values.reserve(2);
      {
        const ThrowingProbe first(alive, copies_left, 10);
        const ThrowingProbe second(alive, copies_left, 20);
        values.push_back(first);
        values.push_back(second);
      }
      require(alive == 2, "only the original two elements remain before failure");
      const auto capacity = values.capacity();
      copies_left = 1;  // 第一份副本成功，第二份副本构造时抛异常。
      bool threw = false;
      try {
        values.reserve(capacity + 1);
      } catch (const CopyFailure&) {
        threw = true;
      }
      require(threw && copies_left == 0, "failure occurs after one successful copy");
      require(values.size() == 2 && values.capacity() == capacity &&
              values[0].value == 10 && values[1].value == 20 && alive == 2,
              "failed reserve cleans partial copies and retains original state");
      copies_left = -1;
      values.reserve(capacity + 1);
      require(values.size() == 2 && values.capacity() >= capacity + 1 &&
              values[0].value == 10 && values[1].value == 20 && alive == 2,
              "container remains usable after reserve fails");
    }
    require(alive == 0, "all throwing test objects are destroyed");
  });

  run("fixed-seed comparison with std::vector<int>", [] {
    constexpr unsigned seed = 336;
    std::mt19937 random(seed);
    std::uniform_int_distribution<int> operation(0, 2);
    std::uniform_int_distribution<int> number(-100, 100);
    std::uniform_int_distribution<int> capacity(0, 64);
    MiniVector<int> actual;
    std::vector<int> expected;
    for (int step = 0; step < 500; ++step) {
      switch (operation(random)) {
        case 0: {
          const int value = number(random);
          actual.push_back(value);
          expected.push_back(value);
          break;
        }
        case 1: {
          const auto requested = static_cast<std::size_t>(capacity(random));
          actual.reserve(requested);
          expected.reserve(requested);
          require(actual.capacity() >= requested, "random reserve meets requested capacity");
          break;
        }
        default:
          actual.clear();
          expected.clear();
      }
      require(actual.size() == expected.size() && actual.size() <= actual.capacity(),
              "random operations preserve size and capacity invariant (seed=336)");
      for (std::size_t i = 0; i < expected.size(); ++i) {
        require(actual[i] == expected[i], "random operations preserve values (seed=336)");
      }
    }
  });

  std::cout << "MiniVector checks: " << passed << " passed, " << failed << " failed\n";
  return failed == 0 ? 0 : 1;
}
