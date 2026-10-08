#pragma once
#include <exception>
#include <iostream>
#include <string_view>

// 只负责运行检查；传入单个 case 名可分阶段验证，默认运行全部。
class ContainerChecks {
public:
  ContainerChecks(int argc, char** argv)
      : selected_(argc == 2 ? argv[1] : "all"), valid_arguments_(argc <= 2) {}

  template <typename Check>
  void run(std::string_view name, Check check) {
    if (!valid_arguments_ || (selected_ != "all" && selected_ != name))
      return;
    ++ran_;
    std::cout << "[RUN] " << name << std::endl;
    try {
      check();
      ++passed_;
      std::cout << "[PASS] " << name << '\n';
    } catch (const std::exception& error) {
      ++failed_;
      std::cerr << "[FAIL] " << name << ": " << error.what() << '\n';
    } catch (...) {
      ++failed_;
      std::cerr << "[FAIL] " << name << ": unexpected exception\n";
    }
  }

  int finish() const {
    if (!valid_arguments_ || ran_ == 0) {
      std::cerr << "Unknown case or invalid arguments; run without arguments for all cases.\n";
      return 2;
    }
    std::cout << "Checks: " << passed_ << " passed, " << failed_ << " failed\n";
    return failed_ == 0 ? 0 : 1;
  }

private:
  std::string_view selected_;
  bool valid_arguments_;
  int ran_ = 0;
  int passed_ = 0;
  int failed_ = 0;
};
