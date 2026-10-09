#include "frame_decoder.hpp"
#include "lab_check.hpp"
#include "lab_exercise_checks.hpp"
#include <algorithm>
#include <array>
#include <iterator>
#include <random>
#include <string_view>

namespace {

using Bytes = std::vector<std::uint8_t>;

// 这是测试数据编码器；不要把它当作待实现 decoder 的算法。
Bytes wire(std::string_view payload) {
  const auto count = static_cast<std::uint32_t>(payload.size());
  Bytes result{static_cast<std::uint8_t>(count >> 24U), static_cast<std::uint8_t>(count >> 16U),
               static_cast<std::uint8_t>(count >> 8U), static_cast<std::uint8_t>(count)};
  result.insert(result.end(), payload.begin(), payload.end());
  return result;
}

template <typename Exception, typename Function>
void expect_throw(Function function, std::string_view message) {
  bool caught = false;
  try {
    function();
  } catch (const Exception&) {
    caught = true;
  }
  require(caught, message);
}

}

int main(int argc, char** argv) {
  LabExerciseChecks checks(argc, argv);
  checks.run("empty_input", [] {
    FrameDecoder decoder;
    require(decoder.feed({}).empty(), "empty feed creates no frame");
    decoder.finish();
    decoder.finish();
  });
  checks.run("complete", [] {
    FrameDecoder decoder;
    require(decoder.feed(wire("QUANT")) == std::vector<std::string>{"QUANT"}, "one complete frame");
    decoder.finish();
  });
  checks.run("every_split", [] {
    const auto bytes = wire("fragmented");
    for (std::size_t split = 0; split <= bytes.size(); ++split) {
      FrameDecoder decoder;
      const auto span = std::span<const std::uint8_t>(bytes);
      auto first = decoder.feed(span.first(split));
      auto second = decoder.feed(span.subspan(split));
      first.insert(first.end(), second.begin(), second.end());
      require(first == std::vector<std::string>{"fragmented"}, "every two-part split works");
      decoder.finish();
    }
  });
  checks.run("bytewise", [] {
    FrameDecoder decoder;
    const auto bytes = wire("abc");
    for (std::size_t index = 0; index < bytes.size(); ++index) {
      const auto result = decoder.feed(std::span<const std::uint8_t>(bytes).subspan(index, 1));
      require(index + 1 == bytes.size() ? result == std::vector<std::string>{"abc"}
                                        : result.empty(),
              "emit only when header and body are complete");
    }
    decoder.finish();
  });
  checks.run("multiple_and_empty", [] {
    auto bytes = wire("one");
    for (const auto payload : {"", "two", ""}) {
      const auto next = wire(payload);
      bytes.insert(bytes.end(), next.begin(), next.end());
    }
    FrameDecoder decoder;
    require(decoder.feed(bytes) == std::vector<std::string>{"one", "", "two", ""},
            "multiple frames and empty frames remain distinct");
    decoder.finish();
  });
  checks.run("binary", [] {
    const std::string payload{'a', '\0', static_cast<char>(0xff), 'z'};
    FrameDecoder decoder;
    require(decoder.feed(wire(payload)) == std::vector<std::string>{payload},
            "embedded zero and high bytes are preserved");
    decoder.finish();
  });
  checks.run("limit", [] {
    const std::string payload(FrameDecoder::max_payload, 'x');
    FrameDecoder decoder;
    require(decoder.feed(wire(payload)) == std::vector<std::string>{payload},
            "the maximum accepted payload includes the boundary");
    decoder.finish();
  });
  checks.run("oversize", [] {
    FrameDecoder decoder;
    // 65537：仅四字节 header 就必须拒绝，不能等待或分配 body。
    const std::array<std::uint8_t, 4> header{0, 1, 0, 1};
    expect_throw<std::length_error>(
        [&] {
          decoder.feed(header);
        },
        "oversize rejected at header");
    expect_throw<std::logic_error>(
        [&] {
          decoder.feed({});
        },
        "failed decoder rejects more input");
    expect_throw<std::logic_error>(
        [&] {
          decoder.finish();
        },
        "failed decoder cannot finish");
  });
  checks.run("truncated_header", [] {
    for (std::size_t count = 1; count < 4; ++count) {
      FrameDecoder decoder;
      const std::array<std::uint8_t, 4> header{0, 0, 0, 5};
      require(decoder.feed(std::span<const std::uint8_t>(header).first(count)).empty(),
              "a partial header emits nothing");
      expect_throw<std::runtime_error>(
          [&] {
            decoder.finish();
          },
          "partial header at EOF is error");
      expect_throw<std::logic_error>(
          [&] {
            decoder.feed({});
          },
          "truncated decoder becomes failed");
    }
  });
  checks.run("truncated_body", [] {
    FrameDecoder decoder;
    const auto bytes = wire("abc");
    require(decoder.feed(std::span<const std::uint8_t>(bytes).first(6)).empty(),
            "partial body emits nothing");
    expect_throw<std::runtime_error>(
        [&] {
          decoder.finish();
        },
        "partial body at EOF is error");
  });
  checks.run("finished", [] {
    FrameDecoder decoder;
    decoder.finish();
    expect_throw<std::logic_error>(
        [&] {
          decoder.feed({});
        },
        "input after EOF is forbidden");
  });
  checks.run("random_chunks", [] {
    const std::vector<std::string> expected{"", "one", std::string(1024, 'z'), "last"};
    Bytes bytes;
    for (const auto& payload : expected) {
      const auto next = wire(payload);
      bytes.insert(bytes.end(), next.begin(), next.end());
    }
    std::mt19937 random(2026);
    for (int trial = 0; trial < 30; ++trial) {
      FrameDecoder decoder;
      std::vector<std::string> actual;
      for (std::size_t offset = 0; offset < bytes.size();) {
        const auto count = std::min<std::size_t>(1 + random() % 31, bytes.size() - offset);
        const auto ready =
            decoder.feed(std::span<const std::uint8_t>(bytes).subspan(offset, count));
        actual.insert(actual.end(), ready.begin(), ready.end());
        offset += count;
      }
      decoder.finish();
      require(actual == expected, "fixed-seed chunks preserve all frames in order");
    }
  });
  return checks.finish();
}
