#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

// 协议：四字节 unsigned big-endian 长度 + 二进制 payload。
// 只实现 TODO；算法与异常路径由你完成，契约详见 README。
class FrameDecoder {
public:
  static constexpr std::size_t max_payload = 64 * 1024;

  // TODO：仅保存不完整帧的状态，按顺序返回本次完成的所有 payload。
  // 空输入合法；空帧返回一个空 string；不能把 payload 当 C 字符串。
  // 超限抛 length_error，并进入永久 failed 状态；failed 后抛 logic_error。
  // 已 finish 的 decoder 不能再 feed（包括空输入），抛 logic_error。
  std::vector<std::string> feed(std::span<const std::uint8_t> bytes) {
    static_cast<void>(bytes);
    throw std::logic_error("TODO: FrameDecoder::feed");
  }

  // TODO：表示输入 EOF。恰在帧边界时成功并关闭输入，可重复调用。
  // 半个 header/body 抛 runtime_error 并进入 failed 状态。
  void finish() {
    throw std::logic_error("TODO: FrameDecoder::finish");
  }

private:
  std::array<std::uint8_t, 4> header_{};
  std::size_t header_used_ = 0;
  std::uint32_t expected_body_ = 0;
  std::string body_;
  bool reading_body_ = false;
  bool failed_ = false;
  bool finished_ = false;
};
