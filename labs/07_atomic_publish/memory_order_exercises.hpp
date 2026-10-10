#pragma once
#include <atomic>
#include <optional>
#include <stdexcept>

// 三线程接力内存序练习，见 EXERCISES.md。核心函数均由你实现。
struct RelayReceipt {
  int source = 0;
  int note = 0;
  bool operator==(const RelayReceipt&) const = default;
};

// 三个角色：唯一 source 发布一次，唯一 relay 转发成功一次，reader 可以有多个。
// stage 只沿 0 -> 1 -> 2 前进，没有 reset；发布/转发后普通字段不再修改。
// owner join 所有使用者后销毁对象。阶段 1 时 reader 还不能读取结果。
class RelayPublication {
public:
  RelayPublication() = default;
  RelayPublication(const RelayPublication&) = delete;
  RelayPublication& operator=(const RelayPublication&) = delete;

  // TODO：写 source_，再发布 stage=1；唯一 source 仅调用一次。
  // 在 stage_ 的 store 操作中直接选择并解释内存序。
  void publish(int source) {
    source_ = source;
    stage_.store(1, std::memory_order_release);
  }

  // TODO：stage 不为 1 时立即返回 nullopt，不修改任何普通字段。
  // 唯一 relay：预检查只用 relaxed；在成功的 acq_rel exchange 之前写 note_，
  // 在 exchange 之后读取 source_ 并返回它。成功后重复调用返回 nullopt。
  // 本练习没有多个 relay 抢占，不需要 CAS；非法多次 source 发布不属于接口契约。
  std::optional<int> try_forward(int note) {
    if (stage_.load(std::memory_order_relaxed) != 1)
      return std::nullopt;
    note_ = note;
    int old = stage_.exchange(2, std::memory_order_acq_rel);
    return std::make_optional(source_);
  }

  // TODO：只有观察到 stage=2 才返回完整 {source_,note_}，其他阶段返回 nullopt。
  // 在 stage_ 的 load 操作中直接选择并解释内存序；允许重复读取，结果不被消耗。
  std::optional<RelayReceipt> try_read() const {
    if (stage_.load(std::memory_order_acquire) != 2)
      return std::nullopt;
    return std::make_optional(RelayReceipt{source_, note_});
  }

private:
  int source_ = 0;
  int note_ = 0;
  std::atomic<int> stage_{0};
};
