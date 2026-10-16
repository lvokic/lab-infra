#pragma once
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <mutex>
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
    static_cast<void>(amount);
    throw std::logic_error("TODO: SharedStats::record");
  }

  // TODO：返回同一个时刻的两个字段；不能把两次独立读取拼成快照。
  Snapshot snapshot() const {
    throw std::logic_error("TODO: SharedStats::snapshot");
  }

private:
  mutable std::mutex mutex_;  // const 查询也需要保护共享状态。
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
    static_cast<void>(value);
    throw std::logic_error("TODO: OneShotResult::publish");
  }

  // TODO：未发布时等待，发布后返回保存的值。
  // 读取不消耗结果；可以反复调用，也允许先 publish 再 wait。
  int wait() {
    throw std::logic_error("TODO: OneShotResult::wait");
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
    throw std::logic_error("TODO: ResourceCounter::add");
  }

  // TODO：取得一份资源时返回 true；无资源且开放时等待。
  // 关闭后仍允许取完已有资源；关闭且无资源时返回 false。
  bool acquire() {
    throw std::logic_error("TODO: ResourceCounter::acquire");
  }

  // TODO：永久关闭，可重复调用；使等待者最终能够退出。
  void close() {
    throw std::logic_error("TODO: ResourceCounter::close");
  }

private:
  std::mutex mutex_;
  std::condition_variable condition_;
  std::size_t available_ = 0;
  bool closed_ = false;
};
