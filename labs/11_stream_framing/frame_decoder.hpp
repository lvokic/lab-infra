#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

// 协议：四字节 unsigned big-endian 长度 + 二进制 payload。
class FrameDecoder {
public:
  static constexpr std::size_t max_payload = 64 * 1024;

  // 仅保存不完整帧的状态，按顺序返回本次完成的所有 payload。
  // 空输入合法；空帧返回一个空 string；不能把 payload 当 C 字符串。
  // 超限抛 length_error，并进入永久 failed 状态；failed 后抛 logic_error。
  // 已 finish 的 decoder 不能再 feed（包括空输入），抛 logic_error。
  std::vector<std::string> feed(std::span<const std::uint8_t> bytes) {
    if (failed_)
      throw std::logic_error("FrameDecoder is in a failed state");
    if (finished_)
      throw std::logic_error("FrameDecoder has already been finished");

    std::vector<std::string> completed_frames;
    for (const std::uint8_t byte : bytes) {
      if (!reading_body_) {
        header_[header_used_++] = byte;
        if (header_used_ < header_.size())
          continue;
        std::uint32_t decode_length = 0;
        for (std::uint8_t len : header_)
          decode_length = (decode_length << 8U) | static_cast<std::uint32_t>(len);
        if (decode_length == 0) {
          header_used_ = 0;
          expected_body_ = 0;
          reading_body_ = false;
          body_.clear();
          completed_frames.push_back(std::string{});
          continue;
        }
        if (decode_length > max_payload) {
          failed_ = true;
          throw std::length_error("FrameDecoder payload exceeds max_payload");
        }
        reading_body_ = true;
        expected_body_ = decode_length;
        body_.clear();
        body_.reserve(expected_body_);
        continue;
      }
      expected_body_--;
      body_.push_back(static_cast<char>(byte));
      if (expected_body_ == 0) {
        completed_frames.push_back(std::move(body_));
        reading_body_ = false;
        header_used_ = 0;
        body_.clear();
      }
    }
    return completed_frames;
  }

  // 表示输入 EOF。恰在帧边界时成功并关闭输入，可重复调用。
  // 半个 header/body 抛 runtime_error 并进入 failed 状态。
  void finish() {
    if (failed_)
      throw std::logic_error("FrameDecoder is in a failed state");
    if (finished_)
      return;
    if (reading_body_) {
      failed_ = true;
      throw std::runtime_error("input ended in the middle of a frame body");
    }
    if (header_used_ != 0) {
      failed_ = true;
      throw std::runtime_error("input ended in the middle of a frame header");
    }
    finished_ = true;
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
