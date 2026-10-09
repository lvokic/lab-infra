#include "atomic_publication.hpp"
#include "lab_check.hpp"
#include "lab_exercise_checks.hpp"
#include <future>
#include <latch>
#include <stop_token>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace {

// 只用于检查退出与异常收集，不参与 writer -> reader 的 payload 发布。
class PublicationWorkers {
public:
  ~PublicationWorkers() {
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

PublicationPayload read_eventually(const OneShotPublication& publication, std::stop_token stop) {
  while (!stop.stop_requested()) {
    if (const auto value = publication.try_read())
      return *value;
    std::this_thread::yield();
  }
  throw std::runtime_error("reader cancelled while unwinding a failed check");
}

PublicationPayload payload(int sequence) {
  return PublicationPayload{sequence, {sequence + 1, -sequence, sequence * 2, 7}};
}

void initially_empty() {
  OneShotPublication publication;
  require(!publication.try_read(), "unpublished object returns nullopt");
}

void writer_first() {
  OneShotPublication publication;
  const auto expected = payload(42);
  publication.publish(expected);
  require(publication.try_read() == expected, "publication before reading is retained");
}

void reader_first() {
  OneShotPublication publication;
  std::latch first_empty(1);
  PublicationWorkers workers;
  auto reader = workers.launch([&](std::stop_token stop) {
    bool was_empty = false;
    try {
      was_empty = !publication.try_read();
    } catch (...) {
      first_empty.count_down();  // TODO 异常也让主线程能够退出检查。
      throw;
    }
    first_empty.count_down();
    const auto received = read_eventually(publication, stop);
    return std::pair{was_empty, received};
  });
  first_empty.wait();  // 只建立 reader 首次检查 -> writer，不是 writer -> 后续 reader。
  const auto expected = payload(17);
  publication.publish(expected);
  const auto [was_empty, received] = reader.get();
  require(was_empty && received == expected, "reader retries and obtains all published fields");
}

void concurrent_start() {
  OneShotPublication publication;
  std::latch start(1);
  const auto expected = payload(-9);
  PublicationWorkers workers;
  auto reader = workers.launch([&](std::stop_token stop) {
    start.wait();
    return read_eventually(publication, stop);
  });
  auto writer = workers.launch([&](std::stop_token) {
    start.wait();
    publication.publish(expected);
  });
  start.count_down();  // 发布前的公共起点不能替代发布所需的同步。
  writer.get();
  const auto received = reader.get();
  require(received == expected, "concurrent reader observes the complete payload");
}

void repeated_reads() {
  OneShotPublication publication;
  const auto expected = payload(3);
  publication.publish(expected);
  for (int index = 0; index < 100; ++index)
    require(publication.try_read() == expected, "immutable publication permits repeated reads");
}

void multiple_readers() {
  OneShotPublication publication;
  std::latch start(1);
  PublicationWorkers workers;
  const auto read = [&](std::stop_token stop) {
    start.wait();
    return read_eventually(publication, stop);
  };
  auto first = workers.launch(read);
  auto second = workers.launch(read);
  const auto expected = payload(101);
  start.count_down();
  publication.publish(expected);
  const auto first_value = first.get();
  const auto second_value = second.get();
  require(first_value == expected && second_value == expected,
          "both readers get the immutable value");
}

void independent_rounds() {
  // 每轮都是新对象；绝不反复切换同一个 ready 并重写普通 payload。
  for (int round = 0; round < 20; ++round) {
    OneShotPublication publication;
    PublicationWorkers workers;
    const auto expected = payload(round);
    auto reader = workers.launch([&](std::stop_token stop) {
      return read_eventually(publication, stop);
    });
    publication.publish(expected);
    require(reader.get() == expected, "each fresh object publishes one coherent payload");
  }
}

}

int main(int argc, char** argv) {
  LabExerciseChecks checks(argc, argv);
  checks.run("initially_empty", initially_empty);
  checks.run("writer_first", writer_first);
  checks.run("reader_first", reader_first);
  checks.run("concurrent_start", concurrent_start);
  checks.run("repeated_reads", repeated_reads);
  checks.run("multiple_readers", multiple_readers);
  checks.run("independent_rounds", independent_rounds);
  return checks.finish();
}
