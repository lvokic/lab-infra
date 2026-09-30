#include "lab_check.hpp"
#include <iostream>
#include <memory>
#include <string>
#include <vector>

struct Plain {
  int value;
};

class Base {
 public:
  explicit Base(std::vector<std::string>& events) : events_(events), construction_value_(value()) {
    events_.push_back("Base constructor");
  }
  Base(const Base&) = default;
  virtual ~Base() { events_.push_back("Base destructor"); }
  virtual int value() const { return 1; }
  int construction_value() const { return construction_value_; }

 protected:
  std::vector<std::string>& events_;

 private:
  int construction_value_;
};

class Sample : public Base {
  public:
    explicit Sample(std::vector<std::string>& events) : Base(events) {}
    int value() const override { return 3; }
};

class Derived final : public Base {
 public:
  explicit Derived(std::vector<std::string>& events) : Base(events) {
    events_.push_back("Derived constructor");
  }
  ~Derived() override { events_.push_back("Derived destructor"); }
  int value() const override { return 2; }
};

int dispatch(Base &sample) {
  return sample.value();
}

int main() {
  std::vector<std::string> events;
  {
    std::unique_ptr<Base> object = std::make_unique<Derived>(events);
    require(object->construction_value() == 1, "Base construction dispatch");
    require(object->value() == 2, "Dynamic dispatch through Base pointer");
    Base sliced = *object;
    std::unique_ptr<Sample> sample = std::make_unique<Sample>(events);
    require(sliced.value() == 1, "Slicing produces a Base object");
    std::cout << "sizeof(Plain)=" << sizeof(Plain)
              << ", sizeof(Base)=" << sizeof(Base)
              << ", sizeof(Derived)=" << sizeof(Derived)
              << ", alignof(Base)=" << alignof(Base)
              << ", dispatched from Base = " << dispatch(sliced)
              << ", dispatched from Derived = " << dispatch(*object)
              << ", dispatched from Derived = " << dispatch(*sample)
              << ", alignof(Derived)=" << alignof(Derived) << '\n';
  }
  // Inspect layout with Clang/LLDB rather than dereferencing a guessed vtable layout.
}
