#pragma once
#include <exception>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

// 学员练习的检查入口：默认全部，也可以指定一个 case；--help 只列名称。
// 与已完成 observer 的 CTest 分开，TODO 失败不会被当作通过。
class LabExerciseChecks {
public:
  LabExerciseChecks(int argc, char** argv)
      : selection_(argc == 2 ? argv[1] : "all"), valid_arguments_(argc <= 2) {}

  template <typename Check>
  void run(std::string_view name, Check check) {
    names_.emplace_back(name);
    if (!valid_arguments_ || selection_ == "--help" || (selection_ != "all" && selection_ != name))
      return;
    ++ran_;
    std::cout << "[RUN] " << name << std::endl;
    try {
      check();
      ++passed_;
      std::cout << "[PASS] " << name << '\n';
    } catch (const std::exception& error) {
      ++failed_;
      std::cout << "[FAIL] " << name << ": " << error.what() << '\n';
    } catch (...) {
      ++failed_;
      std::cout << "[FAIL] " << name << ": unexpected exception\n";
    }
  }

  int finish() const {
    if (!valid_arguments_ || selection_ == "--help" || ran_ == 0) {
      std::cout << "Usage: executable [all|case_name|--help]\n";
      for (const auto& name : names_)
        std::cout << "  " << name << '\n';
      return valid_arguments_ && selection_ == "--help" ? 0 : 2;
    }
    std::cout << "Checks: " << passed_ << " passed, " << failed_ << " failed\n";
    return failed_ == 0 ? 0 : 1;
  }

private:
  std::string_view selection_;
  bool valid_arguments_;
  std::vector<std::string> names_;
  int ran_ = 0;
  int passed_ = 0;
  int failed_ = 0;
};
