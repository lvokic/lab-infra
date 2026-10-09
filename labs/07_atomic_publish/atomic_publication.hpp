#pragma once
#include <array>
#include <atomic>
#include <optional>
#include <stdexcept>

struct PublicationPayload {
  int sequence = 0;
  std::array<int, 4> samples{};

  bool operator==(const PublicationPayload&) const = default;
};

// 一个对象的一生只允许一个 writer 调用一次 publish。
// 允许多个 reader；发布之后 payload 永不修改。没有 reset/第二次发布。
// owner 必须 join 所有使用者后才销毁对象。
class OneShotPublication {
public:
  OneShotPublication() = default;
  OneShotPublication(const OneShotPublication&) = delete;
  OneShotPublication& operator=(const OneShotPublication&) = delete;

  // TODO：发布全部普通字段，使观察到发布状态的 reader 能安全读取它们。
  // 不用 mutex，不把 payload 字段都改为 atomic；内存序由你选择并说明。
  void publish(PublicationPayload value) {
    static_cast<void>(value);
    throw std::logic_error("TODO: OneShotPublication::publish");
  }

  // TODO：未发布时立即返回 nullopt；发布后返回完整 payload 的副本。
  // 这个接口不阻塞；调用者决定是否重试以及何时退出。
  std::optional<PublicationPayload> try_read() const {
    throw std::logic_error("TODO: OneShotPublication::try_read");
  }

private:
  PublicationPayload payload_;
  std::atomic<bool> ready_{false};
};
