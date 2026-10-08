#pragma once
#include <cstddef>
#include <stdexcept>

namespace mini_array_detail {
template <typename T, std::size_t N>
struct Storage {
  T elements[N]{};
};

template <typename T>
struct Storage<T, 0> {};
}

template <typename T, std::size_t N>
class MiniArray {
public:
  using iterator = T*;
  using const_iterator = const T*;

  constexpr std::size_t size() const noexcept {
    return N;
  }

  constexpr bool empty() const noexcept {
    return N == 0;
  }

  // TODO：N=0 时 data/begin/end 都返回 nullptr，不能对空指针做加法。
  T* data() {
    if constexpr (N == 0) {
      return nullptr;
    } else {
      return storage_.elements;
    }
  }

  const T* data() const {
    if constexpr (N == 0) {
      return nullptr;
    } else {
      return storage_.elements;
    }
  }

  iterator begin() {
    if constexpr (N == 0) {
      return nullptr;
    } else {
      return storage_.elements;
    }
  }

  iterator end() {
    if constexpr (N == 0) {
      return nullptr;
    } else {
      return storage_.elements + N;
    }
  }

  const_iterator begin() const {
    if constexpr (N == 0) {
      return nullptr;
    } else {
      return storage_.elements;
    }
  }

  const_iterator end() const {
    if constexpr (N == 0) {
      return nullptr;
    } else {
      return storage_.elements + N;
    }
  }

  T& operator[](std::size_t index) {
    return at(index);
  }

  const T& operator[](std::size_t index) const {
    return at(index);
  }

  // TODO：at 越界，或者 N=0 时 front/back，均抛 out_of_range。
  T& at(std::size_t index) {
    if constexpr (N == 0) {
      throw std::out_of_range("MiniArray index out of range");
    } else {
      if (index >= N)
        throw std::out_of_range("MiniArray index out of range");
      return *(storage_.elements + index);
    }
  }

  const T& at(std::size_t index) const {
    if constexpr (N == 0) {
      throw std::out_of_range("MiniArray index out of range");
    } else {
      if (index >= N)
        throw std::out_of_range("MiniArray index out of range");
      return *(storage_.elements + index);
    }
  }

  T& front() {
    if constexpr (N == 0)
      throw std::out_of_range("MiniArray index out of range");
    return *begin();
  }

  const T& front() const {
    if constexpr (N == 0)
      throw std::out_of_range("MiniArray index out of range");
    return *begin();
  }

  T& back() {
    if constexpr (N == 0)
      throw std::out_of_range("MiniArray index out of range");
    return *(end() - 1);
  }

  const T& back() const {
    if constexpr (N == 0)
      throw std::out_of_range("MiniArray index out of range");
    return *(end() - 1);
  }

  void fill(const T& value) {
    for (std::size_t i = 0; i < N; ++i) {
      at(i) = value;
    }
  }

private:
  mini_array_detail::Storage<T, N> storage_;
  // 拷贝/移动/析构采用成员的默认行为；不需要手写资源管理。
};
