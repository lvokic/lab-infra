#pragma once
#include "lab_check.hpp"
#include <chrono>
#include <cstdint>
#include <exception>
#include <stop_token>
#include <thread>

namespace lockfree_driver {

// 已完成的调用端：重试策略、退出与校验，不包含队列算法。
// 操作自己挂死时仍需 README 中的外部 timeout。
template <typename Queue>
std::uint64_t transfer(Queue& queue, int count) {
  require(count >= 0, "transfer count is nonnegative");
  std::stop_source abort;
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(4);
  std::exception_ptr writer_error, reader_error;
  std::uint64_t checksum = 0;
  const auto keep_running = [&](std::stop_token token) {
    require(!token.stop_requested() && !abort.stop_requested(), "transfer was cancelled");
    require(std::chrono::steady_clock::now() < deadline, "transfer retry deadline exceeded");
  };
  std::jthread writer([&](std::stop_token token) {
    try {
      for (int value = 0; value < count; ++value) {
        while (!queue.try_push(value)) {
          keep_running(token);
          std::this_thread::yield();
        }
      }
    } catch (...) {
      writer_error = std::current_exception();
      abort.request_stop();
    }
  });
  std::jthread reader([&](std::stop_token token) {
    try {
      for (int expected = 0; expected < count; ++expected) {
        auto value = queue.try_pop();
        while (!value) {
          keep_running(token);
          std::this_thread::yield();
          value = queue.try_pop();
        }
        require(*value == expected, "SPSC transfer preserves exact FIFO values");
        checksum += static_cast<std::uint64_t>(*value);
      }
    } catch (...) {
      reader_error = std::current_exception();
      abort.request_stop();
    }
  });
  writer.join();
  reader.join();
  if (writer_error)
    std::rethrow_exception(writer_error);
  if (reader_error)
    std::rethrow_exception(reader_error);
  require(!queue.try_pop(), "transfer leaves no duplicate or extra element");
  const auto n = static_cast<std::uint64_t>(count);
  require(checksum == (n == 0 ? 0 : n * (n - 1) / 2), "transfer checksum");
  return checksum;
}

}  // namespace lockfree_driver
