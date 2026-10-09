#pragma once
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <shared_mutex>
#include <stdexcept>

// 只实现本文件中的 TODO；同步算法见练习要求，由你自己写。
// 三个对象都不支持复制/移动；销毁前必须 join 所有访问它的线程。

class SharedStats {
public:
  struct Snapshot {
    std::size_t tasks = 0;
    std::int64_t total_amount = 0;
  };

  SharedStats() = default;
  SharedStats(const SharedStats&) = delete;
  SharedStats& operator=(const SharedStats&) = delete;

  // TODO：记录一项任务，同时更新任务数和总金额。
  // 本练习输入为非负金额，累计值不会溢出。
  void record(std::int64_t amount) {
    std::unique_lock lock(mutex_);
    tasks_++;
    total_amount_ += amount;
  }

  // TODO：返回同一个时刻的两个字段；不能把两次独立读取拼成快照。
  Snapshot snapshot() const {
    std::shared_lock lock(mutex_);
    return {tasks_, total_amount_};
  }

private:
  mutable std::shared_mutex mutex_;
  std::size_t tasks_ = 0;
  std::int64_t total_amount_ = 0;
};

class OneShotResult {
public:
  OneShotResult() = default;
  OneShotResult(const OneShotResult&) = delete;
  OneShotResult& operator=(const OneShotResult&) = delete;

  // TODO：第一次发布保存 value 并返回 true，后续发布返回 false。
  // 可有多个等待者；成功发布后它们都应能结束等待。
  bool publish(int value) {
    {
      std::unique_lock<std::mutex> lock(mutex_);
      if (ready_) {
        return false;
      }
      value_ = value;
      ready_ = true;
    }
    condition_.notify_all();
    return true;
  }

  // TODO：未发布时等待，发布后返回保存的值。
  // 读取不消耗结果；可以反复调用，也允许先 publish 再 wait。
  int wait() {
    std::unique_lock<std::mutex> lock(mutex_);
    condition_.wait(lock, [this] {
      return ready_;
    });
    return value_;
  }

private:
  std::mutex mutex_;
  std::condition_variable condition_;
  bool ready_ = false;
  int value_ = 0;
};

class ResourceCounter {
public:
  ResourceCounter() = default;
  ResourceCounter(const ResourceCounter&) = delete;
  ResourceCounter& operator=(const ResourceCounter&) = delete;

  // TODO：开放时添加一份资源并返回 true；关闭后返回 false。
  // 本练习添加的总份数不会使 available_ 溢出；不设置容量上限。
  bool add() {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      if (closed_)
        return false;
      ++available_;
    }
    condition_.notify_one();
    return true;
  }

  // TODO：取得一份资源时返回 true；无资源且开放时等待。
  // 关闭后仍允许取完已有资源；关闭且无资源时返回 false。
  bool acquire() {
    std::unique_lock<std::mutex> lock(mutex_);
    condition_.wait(lock, [this] {
      return available_ > 0 || closed_;
    });
    if (available_ == 0)
      return false;
    available_--;
    return true;
  }

  // TODO：永久关闭，可重复调用；使等待者最终能够退出。
  void close() {
    {
      std::scoped_lock<std::mutex> lock(mutex_);
      closed_ = true;
    }
    condition_.notify_all();
  }

private:
  std::mutex mutex_;
  std::condition_variable condition_;
  std::size_t available_ = 0;
  bool closed_ = false;
};
