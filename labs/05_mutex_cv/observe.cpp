#include "lab_check.hpp"
#include <chrono>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <thread>

int main() {
  std::mutex mutex;
  std::condition_variable condition;
  bool ready = false;
  int payload = 0;
  int observed = 0;
  bool timed_out = false;

  std::thread reader([&] {
    std::unique_lock lock(mutex);
    const bool signaled = condition.wait_for(lock, std::chrono::seconds(2), [&] { return ready; });
    timed_out = !signaled;
    if (signaled) {
      observed = payload;
    }
  });

  condition.notify_one();  // A notification alone does not make ready true.
  {
    std::lock_guard lock(mutex);
    payload = 42;
    ready = true;
  }
  condition.notify_one();
  reader.join();  // Publishes the reader's results to the joining thread.

  require(!timed_out && observed == 42, "The protected predicate and payload must agree");
  std::cout << "Observed payload=" << observed << "; predicate wait completed.\n";
  // Extend this into a bounded queue using the contract in labs/06_bounded_queue/README.md.
}
