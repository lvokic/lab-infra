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
    throw std::logic_error("TODO: MiniArray::data");
  }

  const T* data() const {
    throw std::logic_error("TODO: MiniArray::data const");
  }

  iterator begin() {
    throw std::logic_error("TODO: MiniArray::begin");
  }

  iterator end() {
    throw std::logic_error("TODO: MiniArray::end");
  }

  const_iterator begin() const {
    throw std::logic_error("TODO: MiniArray::begin const");
  }

  const_iterator end() const {
    throw std::logic_error("TODO: MiniArray::end const");
  }

  T& operator[](std::size_t index) {
    static_cast<void>(index);
    throw std::logic_error("TODO: MiniArray::operator[]");
  }

  const T& operator[](std::size_t index) const {
    static_cast<void>(index);
    throw std::logic_error("TODO: MiniArray::operator[] const");
  }

  // TODO：at 越界，或者 N=0 时 front/back，均抛 out_of_range。
  T& at(std::size_t /*index*/) {
    throw std::logic_error("TODO: MiniArray::at");
  }

  const T& at(std::size_t /*index*/) const {
    throw std::logic_error("TODO: MiniArray::at const");
  }

  T& front() {
    throw std::logic_error("TODO: MiniArray::front");
  }

  const T& front() const {
    throw std::logic_error("TODO: MiniArray::front const");
  }

  T& back() {
    throw std::logic_error("TODO: MiniArray::back");
  }

  const T& back() const {
    throw std::logic_error("TODO: MiniArray::back const");
  }

  void fill(const T& /*value*/) {
    throw std::logic_error("TODO: MiniArray::fill");
  }

private:
  mini_array_detail::Storage<T, N> storage_;
  // 拷贝/移动/析构采用成员的默认行为；不需要手写资源管理。
};
