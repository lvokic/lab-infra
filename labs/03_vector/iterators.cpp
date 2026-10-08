#include "lab_check.hpp"
#include <iostream>
#include <string_view>
#include <vector>

void show(std::string_view stage, const std::vector<int>& values) {
  std::cout << '[' << stage << "] size=" << values.size() << " capacity=" << values.capacity()
            << " data=" << static_cast<const void*>(values.data()) << " values=[";
  std::string_view separator;
  // begin 指向第一个元素；end 是尾后位置。必须先判断 it != end 才能解引用。
  for (auto it = values.begin(); it != values.end(); ++it) {
    std::cout << separator << *it;
    separator = ", ";
  }
  std::cout << "]\n";
}

int main() {
  // 1. 在容量足够的情况下，观察中间插入和删除。
  {
    std::vector<int> values{10, 20, 30};
    values.reserve(8);  // 先留出空间，再取得迭代器、指针和引用。
    auto first = values.begin();
    int* first_pointer = &values.front();
    int& first_reference = values.front();
    const auto* storage = values.data();
    show("initial", values);
    std::cout << "*begin=" << *first << " *(begin + 1)=" << *(values.begin() + 1) << '\n';

    {
      auto position = values.begin() + 1;           // 指向 20。
      auto inserted = values.insert(position, 15);  // 在 20 前面插入 15。
      // position 已失效；insert 返回一个指向新元素的有效迭代器。
      show("insert 15 before 20", values);
      std::cout << "*inserted=" << *inserted << '\n';
      require(values == std::vector<int>{10, 15, 20, 30} && inserted == values.begin() + 1 &&
                  *inserted == 15,
              "insert returns the new element's position");
    }
    require(values.data() == storage && *first == 10 && *first_pointer == 10 &&
                first_reference == 10,
            "without reallocation, handles before the insertion remain valid");

    {
      auto position = values.begin() + 2;  // 重新获取位置，现在指向 20。
      auto next = values.erase(position);  // 删除 20，后面的 30 向前补位。
      // position 已失效；erase 返回删除位置之后的元素的新位置。
      show("erase 20", values);
      std::cout << "*next=" << *next << '\n';
      require(values == std::vector<int>{10, 15, 30} && next == values.begin() + 2 && *next == 30,
              "erase returns the next surviving element's position");
    }
    require(*first == 10 && *first_pointer == 10 && first_reference == 10,
            "handles before the erased position remain valid");

    auto next = values.erase(values.end() - 1);  // 删除最后一个元素 30。
    show("erase last element", values);
    require(next == values.end() && values == std::vector<int>{10, 15},
            "erasing the last element returns the new end");
    std::cout << "next == end: true; do not dereference next\n";
  }

  // 2. 尾部追加但不扩容：已有元素的迭代器有效，旧 end 失效。
  {
    std::vector<int> values{10, 20};
    values.reserve(4);
    auto first = values.begin();
    const auto* storage = values.data();
    show("before append", values);
    std::cout << "end - begin=" << values.end() - values.begin() << '\n';
    values.push_back(30);
    show("append without reallocation", values);
    require(values.data() == storage && *first == 10 && values == std::vector<int>{10, 20, 30},
            "append without reallocation preserves existing element iterators");
    // 每次都重新调用 end()，不保存和使用追加前的尾后迭代器。
    std::cout << "*first=" << *first << " new end - begin=" << values.end() - values.begin()
              << '\n';
  }

  // 3. 强制扩容：所有旧元素迭代器失效，重新从容器取得迭代器。
  {
    std::vector<int> values{10, 20};
    {
      auto old_first = values.begin();
      show("before reallocation", values);
      std::cout << "*old_first=" << *old_first << '\n';
      values.reserve(values.capacity() + 1);
      // old_first 已失效。从这里开始不解引用、不比较、不递增它。
    }
    show("after reallocation", values);
    auto new_first = values.begin();
    require(*new_first == 10 && values == std::vector<int>{10, 20},
            "fresh iterators access the relocated elements");
    std::cout << "*new_first=" << *new_first << '\n';
  }

  // 4. 遍历时删除：接住 erase 的返回值，避免继续使用失效的迭代器。
  {
    std::vector<int> values{1, 2, 3, 4, 5, 6};
    show("before removing even numbers", values);
    for (auto it = values.begin(); it != values.end();) {
      if (*it % 2 == 0) {
        it = values.erase(it);  // 已指向下一个元素，这一轮不再 ++it。
      } else {
        ++it;
      }
    }
    show("after removing even numbers", values);
    require(values == std::vector<int>{1, 3, 5},
            "erasing during traversal neither skips elements nor uses invalid iterators");
  }
}
