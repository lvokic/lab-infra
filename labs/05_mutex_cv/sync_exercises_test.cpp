#include "lab_check.hpp"
#include "sync_exercises.hpp"
#include <array>
#include <exception>
#include <future>
#include <iostream>
#include <latch>
#include <string_view>
#include <utility>

namespace {

// std::async 将工作线程的异常保存在 future 里，get() 再交给检查入口报告。
template <typename Function>
auto worker(Function function) {
  return std::async(std::launch::async, std::move(function));
}

void stats_initial() {
  SharedStats stats;
  const auto state = stats.snapshot();
  require(state.tasks == 0 && state.total_amount == 0, "initial snapshot is zero");
}

void stats_records() {
  SharedStats stats;
  stats.record(7);
  stats.record(0);
  stats.record(13);
  const auto state = stats.snapshot();
  require(state.tasks == 3 && state.total_amount == 20, "record updates both fields");
}

void stats_concurrent() {
  SharedStats stats;
  std::latch start(1);
  const auto record_many = [&] {
    start.wait();
    for (int index = 0; index < 2000; ++index)
      stats.record(7);
  };
  auto first = worker(record_many);
  auto second = worker(record_many);
  start.count_down();
  // 主线程同时读取；任何一次快照都必须保持关联字段的关系。
  bool consistent = true;
  for (int index = 0; index < 2000; ++index) {
    const auto state = stats.snapshot();
    consistent = consistent && state.total_amount == static_cast<std::int64_t>(state.tasks) * 7;
  }
  first.get();
  second.get();
  const auto state = stats.snapshot();
  require(consistent, "concurrent snapshots preserve amount == tasks * 7");
  require(state.tasks == 4000 && state.total_amount == 28000, "no lost records");
}

void result_early() {
  OneShotResult result;
  require(result.publish(42), "first publication succeeds");
  require(result.wait() == 42 && result.wait() == 42, "early publication and repeated reads");
}

void result_duplicate() {
  OneShotResult result;
  const bool first = result.publish(17);
  const bool second = result.publish(99);
  require(first && !second && result.wait() == 17, "duplicate publication keeps the first value");
}

void result_waiters() {
  OneShotResult result;
  std::latch started(2);
  const auto read = [&] {
    started.count_down();
    return result.wait();
  };
  auto first = worker(read);
  auto second = worker(read);
  started.wait();
  const bool published = result.publish(-7);
  const int first_value = first.get();
  const int second_value = second.get();
  require(published && first_value == -7 && second_value == -7,
          "every reader receives the published value");
}

void result_publishers() {
  OneShotResult result;
  std::latch start(1);
  auto first = worker([&] {
    start.wait();
    return result.publish(11);
  });
  auto second = worker([&] {
    start.wait();
    return result.publish(22);
  });
  start.count_down();
  const bool first_won = first.get();
  const bool second_won = second.get();
  require(first_won != second_won, "exactly one publisher succeeds");
  require(result.wait() == (first_won ? 11 : 22), "stored result belongs to the winner");
}

void resources_drain() {
  ResourceCounter resources;
  const bool first = resources.add();
  const bool second = resources.add();
  resources.close();
  resources.close();
  const bool rejected = !resources.add();
  const bool acquired_first = resources.acquire();
  const bool acquired_second = resources.acquire();
  const bool exhausted = !resources.acquire();
  require(first && second && rejected, "close is permanent and rejects additions");
  require(acquired_first && acquired_second && exhausted, "close preserves pending resources");
}

void resources_closed_empty() {
  ResourceCounter resources;
  resources.close();
  require(!resources.acquire() && !resources.acquire(), "closed empty counter never waits");
}

void resources_wake() {
  ResourceCounter resources;
  std::latch started(1);
  auto reader = worker([&] {
    started.count_down();
    return resources.acquire();
  });
  started.wait();
  const bool added = resources.add();
  // 在 close 前等这次消费完成，避免 close 掩盖 add 遗漏通知的问题。
  const bool acquired = reader.get();
  resources.close();
  require(added && acquired && !resources.acquire(), "one addition releases one acquisition");
}

void resources_close_waiters() {
  ResourceCounter resources;
  std::latch started(2);
  const auto read = [&] {
    started.count_down();
    return resources.acquire();
  };
  auto first = worker(read);
  auto second = worker(read);
  started.wait();
  resources.close();
  const bool first_value = first.get();
  const bool second_value = second.get();
  require(!first_value && !second_value, "closing empty counter releases all readers");
}

void resources_concurrent() {
  ResourceCounter resources;
  std::latch started(2);
  const auto consume = [&] {
    started.count_down();
    int consumed = 0;
    while (resources.acquire())
      ++consumed;
    return consumed;
  };
  auto first = worker(consume);
  auto second = worker(consume);
  started.wait();
  bool accepted = true;
  for (int index = 0; index < 100; ++index)
    accepted = resources.add() && accepted;
  resources.close();
  const int first_count = first.get();
  const int second_count = second.get();
  require(accepted && first_count + second_count == 100,
          "resources are neither lost nor duplicated");
  require(!resources.acquire(), "all resources have been drained");
}

struct Check {
  std::string_view name;
  void (*run)();
};

constexpr std::array checks{
    Check{"stats_initial", stats_initial},
    Check{"stats_records", stats_records},
    Check{"stats_concurrent", stats_concurrent},
    Check{"result_early", result_early},
    Check{"result_duplicate", result_duplicate},
    Check{"result_waiters", result_waiters},
    Check{"result_publishers", result_publishers},
    Check{"resources_drain", resources_drain},
    Check{"resources_closed_empty", resources_closed_empty},
    Check{"resources_wake", resources_wake},
    Check{"resources_close_waiters", resources_close_waiters},
    Check{"resources_concurrent", resources_concurrent},
};

bool matches(std::string_view selection, std::string_view name) {
  return selection == "all" || selection == name ||
         (selection == "stats" && name.starts_with("stats_")) ||
         (selection == "result" && name.starts_with("result_")) ||
         (selection == "resources" && name.starts_with("resources_"));
}

void help() {
  std::cout << "Usage: sync_exercises_test [all|stats|result|resources|case_name]\n";
  for (const auto& check : checks)
    std::cout << "  " << check.name << '\n';
}

}

int main(int argc, char** argv) {
  if (argc > 2) {
    help();
    return 2;
  }
  const std::string_view selection = argc == 2 ? argv[1] : "all";
  if (selection == "--help") {
    help();
    return 0;
  }
  int passed = 0;
  int failed = 0;
  for (const auto& check : checks) {
    if (!matches(selection, check.name))
      continue;
    std::cout << "[RUN] " << check.name << std::endl;
    try {
      check.run();
      ++passed;
      std::cout << "[PASS] " << check.name << '\n';
    } catch (const std::exception& error) {
      ++failed;
      std::cout << "[FAIL] " << check.name << ": " << error.what() << '\n';
    } catch (...) {
      ++failed;
      std::cout << "[FAIL] " << check.name << ": unexpected exception\n";
    }
  }
  if (passed + failed == 0) {
    help();
    return 2;
  }
  std::cout << "Checks: " << passed << " passed, " << failed << " failed\n";
  return failed == 0 ? 0 : 1;
}
