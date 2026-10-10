#include "lab_check.hpp"
#include "lab_exercise_checks.hpp"
#include "memory_order_exercises.hpp"
#include <future>
#include <latch>
#include <stop_token>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace {

// 测试支撑只管理线程退出和异常，不参与 source -> relay -> reader 的数据发布。
class RelayWorkers {
public:
  ~RelayWorkers() {
    for (auto& thread : threads_)
      thread.request_stop();
    threads_.clear();
  }

  template <typename Function>
  auto launch(Function function) {
    using Result = std::invoke_result_t<Function, std::stop_token>;
    std::packaged_task<Result(std::stop_token)> task(std::move(function));
    auto result = task.get_future();
    threads_.emplace_back([task = std::move(task)](std::stop_token stop) mutable {
      task(stop);
    });
    return result;
  }

private:
  std::vector<std::jthread> threads_;
};

RelayReceipt read_eventually(const RelayPublication& publication, std::stop_token stop) {
  while (!stop.stop_requested()) {
    if (auto result = publication.try_read())
      return *result;
    std::this_thread::yield();
  }
  throw std::runtime_error("reader cancelled while unwinding a failed check");
}

int forward_eventually(RelayPublication& publication, int note, std::stop_token stop) {
  while (!stop.stop_requested()) {
    if (auto result = publication.try_forward(note))
      return *result;
    std::this_thread::yield();
  }
  throw std::runtime_error("relay cancelled while unwinding a failed check");
}

void concurrent_round(int source, int note, bool two_readers) {
  RelayPublication publication;
  std::latch start(1);
  RelayWorkers workers;
  // 全部工作线程先创建，再开始普通字段的写入，避免用线程启动掩盖缺失的发布关系。
  auto reader = workers.launch([&](std::stop_token stop) {
    start.wait();
    return read_eventually(publication, stop);
  });
  std::future<RelayReceipt> second;
  if (two_readers)
    second = workers.launch([&](std::stop_token stop) {
      start.wait();
      return read_eventually(publication, stop);
    });
  auto relay = workers.launch([&](std::stop_token stop) {
    start.wait();
    return forward_eventually(publication, note, stop);
  });
  auto writer = workers.launch([&](std::stop_token) {
    start.wait();
    publication.publish(source);
  });
  start.count_down();
  writer.get();
  require(relay.get() == source, "relay acquires the source value");
  const RelayReceipt expected{source, note};
  require(reader.get() == expected, "reader acquires both source and relay note");
  if (two_readers)
    require(second.get() == expected, "multiple readers receive the same immutable result");
}

}

int main(int argc, char** argv) {
  LabExerciseChecks checks(argc, argv);
  checks.run("initially_empty", [] {
    RelayPublication publication;
    require(!publication.try_read() && !publication.try_forward(7), "stage zero has no data");
  });
  checks.run("source_only", [] {
    RelayPublication publication;
    publication.publish(42);
    require(!publication.try_read(), "source publication alone does not complete the relay");
  });
  checks.run("forward_once", [] {
    RelayPublication publication;
    publication.publish(42);
    require(publication.try_forward(7) == 42, "relay obtains the source value");
    require(publication.try_read() == RelayReceipt{42, 7}, "receipt includes relay note");
    require(!publication.try_forward(99), "relay cannot forward a second time");
    for (int index = 0; index < 20; ++index)
      require(publication.try_read() == RelayReceipt{42, 7},
              "repeated reads preserve the first note");
  });
  checks.run("early_relay", [] {
    RelayPublication publication;
    require(!publication.try_forward(99), "early attempt does not publish or reserve a result");
    publication.publish(-3);
    require(publication.try_forward(5) == -3 && publication.try_read() == RelayReceipt{-3, 5},
            "failed early attempt does not replace the successful note");
  });
  checks.run("concurrent_start", [] {
    concurrent_round(42, 7, false);
  });
  checks.run("multiple_readers", [] {
    concurrent_round(-9, 13, true);
  });
  checks.run("independent_rounds", [] {
    for (int round = 0; round < 20; ++round)
      concurrent_round(round, -round, false);
  });
  return checks.finish();
}
