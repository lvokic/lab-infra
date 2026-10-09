#include "bounded_queue.hpp"
#include "lab_check.hpp"
#include "lab_exercise_checks.hpp"
#include <algorithm>
#include <future>
#include <latch>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace {

// packaged_task 收集异常；退出检查时先关闭队列，再 join，最后才销毁队列。
// 错误的 close/wait 实现仍可能挂死，因此 README 要求外部 timeout。
class QueueWorkers {
public:
  explicit QueueWorkers(BoundedQueue& queue) : queue_(queue) {}

  ~QueueWorkers() {
    try {
      queue_.close();
    } catch (...) {
      // 未实现的 close 会抛 TODO；不能让测试清理触发 terminate。
    }
    threads_.clear();
  }

  template <typename Function>
  auto launch(Function function) {
    using Result = std::invoke_result_t<Function>;
    std::packaged_task<Result()> task(std::move(function));
    auto result = task.get_future();
    threads_.emplace_back(std::move(task));
    return result;
  }

private:
  BoundedQueue& queue_;
  std::vector<std::jthread> threads_;
};

void capacity_zero() {
  bool rejected = false;
  try {
    BoundedQueue queue(0);
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  require(rejected, "zero capacity is rejected with invalid_argument");
}

void fifo() {
  BoundedQueue queue(3);
  require(queue.push(10) && queue.push(-7) && queue.push(30), "open queue accepts three values");
  require(queue.pop() == 10 && queue.pop() == -7 && queue.pop() == 30, "pop follows FIFO");
  queue.close();
  require(!queue.pop(), "closed drained queue returns nullopt");
}

void close_empty() {
  BoundedQueue queue(1);
  queue.close();
  queue.close();
  require(!queue.push(42) && !queue.pop() && !queue.pop(), "closed empty queue never waits");
}

void close_drain() {
  BoundedQueue queue(2);
  require(queue.push(11) && queue.push(22), "fill queue before closing");
  queue.close();
  require(!queue.push(33), "closed full queue rejects without waiting");
  require(queue.pop() == 11 && queue.pop() == 22 && !queue.pop(), "close preserves queued values");
}

void reader_wake() {
  BoundedQueue queue(1);
  std::latch started(1);
  QueueWorkers workers(queue);
  auto reader = workers.launch([&] {
    started.count_down();
    return queue.pop();
  });
  started.wait();
  const bool accepted = queue.push(42);
  const auto value = reader.get();  // close 之前完成，避免 close 掩盖 push 遗漏通知。
  require(accepted && value == 42, "push allows an empty-queue reader to finish");
}

void writer_wake() {
  BoundedQueue queue(1);
  require(queue.push(10), "fill capacity-one queue");
  std::latch started(1);
  QueueWorkers workers(queue);
  auto writer = workers.launch([&] {
    started.count_down();
    return queue.push(20);
  });
  started.wait();
  const auto first = queue.pop();
  const bool accepted = writer.get();  // 不靠 close 帮助生产者结束等待。
  const auto second = queue.pop();
  require(first == 10 && accepted && second == 20, "pop makes room for a waiting writer");
}

void close_readers() {
  BoundedQueue queue(1);
  std::latch started(2);
  QueueWorkers workers(queue);
  const auto read = [&] {
    started.count_down();
    return queue.pop();
  };
  auto first = workers.launch(read);
  auto second = workers.launch(read);
  started.wait();
  queue.close();
  const auto first_value = first.get();
  const auto second_value = second.get();
  require(!first_value && !second_value, "close releases every empty-queue reader");
}

void close_writers() {
  BoundedQueue queue(1);
  require(queue.push(10), "fill queue before starting writers");
  std::latch started(2);
  QueueWorkers workers(queue);
  const auto write = [&] {
    started.count_down();
    return queue.push(20);
  };
  auto first = workers.launch(write);
  auto second = workers.launch(write);
  started.wait();
  queue.close();
  const bool first_accepted = first.get();
  const bool second_accepted = second.get();
  require(!first_accepted && !second_accepted, "close rejects every full-queue writer");
  require(queue.pop() == 10 && !queue.pop(), "rejected writers do not change pending data");
}

void transfer(std::size_t capacity) {
  BoundedQueue queue(capacity);
  QueueWorkers workers(queue);
  auto reader = workers.launch([&] {
    std::vector<int> values;
    while (const auto value = queue.pop())
      values.push_back(*value);
    return values;
  });
  auto writer = workers.launch([&] {
    bool accepted = true;
    for (int value = 0; value < 200; ++value)
      accepted = queue.push(value) && accepted;
    return accepted;
  });
  const bool accepted = writer.get();
  queue.close();
  const auto values = reader.get();
  require(accepted && values.size() == 200, "all single-producer values arrive");
  for (int value = 0; value < 200; ++value)
    require(values[static_cast<std::size_t>(value)] == value,
            "single producer preserves exact order");
}

void multi_producer_order() {
  constexpr int per_producer = 100;
  BoundedQueue queue(2);
  QueueWorkers workers(queue);
  auto reader = workers.launch([&] {
    std::vector<int> values;
    while (const auto value = queue.pop())
      values.push_back(*value);
    return values;
  });
  const auto write = [&](int producer) {
    bool accepted = true;
    for (int sequence = 0; sequence < per_producer; ++sequence)
      accepted = queue.push(producer * per_producer + sequence) && accepted;
    return accepted;
  };
  auto first = workers.launch([write] {
    return write(0);
  });
  auto second = workers.launch([write] {
    return write(1);
  });
  const bool first_accepted = first.get();
  const bool second_accepted = second.get();
  queue.close();
  const auto values = reader.get();
  require(first_accepted && second_accepted && values.size() == 200, "both producers finish");
  int next[2] = {0, 0};
  for (const int value : values) {
    require(value >= 0 && value < 200, "value belongs to a producer");
    const int producer = value / per_producer;
    require(value % per_producer == next[producer], "one consumer observes each producer in order");
    ++next[producer];
  }
  require(next[0] == per_producer && next[1] == per_producer, "no missing producer values");
}

void multi_consumer_ids() {
  BoundedQueue queue(2);
  QueueWorkers workers(queue);
  const auto read = [&] {
    std::vector<int> values;
    while (const auto value = queue.pop())
      values.push_back(*value);
    return values;
  };
  auto first_reader = workers.launch(read);
  auto second_reader = workers.launch(read);
  const auto write = [&](int begin) {
    bool accepted = true;
    for (int value = begin; value < begin + 100; ++value)
      accepted = queue.push(value) && accepted;
    return accepted;
  };
  auto first_writer = workers.launch([write] {
    return write(0);
  });
  auto second_writer = workers.launch([write] {
    return write(100);
  });
  const bool first_accepted = first_writer.get();
  const bool second_accepted = second_writer.get();
  queue.close();
  auto values = first_reader.get();
  const auto second_values = second_reader.get();
  values.insert(values.end(), second_values.begin(), second_values.end());
  std::sort(values.begin(), values.end());
  require(first_accepted && second_accepted && values.size() == 200, "every unique ID is consumed");
  for (int value = 0; value < 200; ++value)
    require(values[static_cast<std::size_t>(value)] == value,
            "IDs are neither lost nor duplicated");
}

}

int main(int argc, char** argv) {
  LabExerciseChecks checks(argc, argv);
  checks.run("capacity_zero", capacity_zero);
  checks.run("fifo", fifo);
  checks.run("close_empty", close_empty);
  checks.run("close_drain", close_drain);
  checks.run("reader_wake", reader_wake);
  checks.run("writer_wake", writer_wake);
  checks.run("close_readers", close_readers);
  checks.run("close_writers", close_writers);
  checks.run("transfer_one", [] {
    transfer(1);
  });
  checks.run("transfer_two", [] {
    transfer(2);
  });
  checks.run("multi_producer_order", multi_producer_order);
  checks.run("multi_consumer_ids", multi_consumer_ids);
  return checks.finish();
}
