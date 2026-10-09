#include "event_connection.hpp"
#include "lab_check.hpp"
#include "lab_exercise_checks.hpp"
#include <array>
#include <fcntl.h>
#include <unistd.h>

namespace {

// 完整测试支撑：创建本地非阻塞 fd；不实现待练习的事件循环。
class LocalPair {
public:
  LocalPair() {
    require(::socketpair(AF_UNIX, SOCK_STREAM, 0, sockets_.data()) == 0, "create local socketpair");
    for (const int fd : sockets_) {
      const int flags = ::fcntl(fd, F_GETFL, 0);
      if (flags < 0 || ::fcntl(fd, F_SETFL, flags | O_NONBLOCK) != 0) {
        for (const int owned : sockets_)
          ::close(owned);
        throw std::runtime_error("set test sockets nonblocking");
      }
    }
  }

  LocalPair(const LocalPair&) = delete;
  LocalPair& operator=(const LocalPair&) = delete;

  ~LocalPair() {
    for (const int fd : sockets_)
      ::close(fd);
  }

  int server() const {
    return sockets_[0];
  }

  int peer() const {
    return sockets_[1];
  }

private:
  std::array<int, 2> sockets_{};
};

void peer_write(int fd, std::string_view bytes) {
  const auto sent = ::send(fd, bytes.data(), bytes.size(), MSG_NOSIGNAL);
  require(sent >= 0 && static_cast<std::size_t>(sent) == bytes.size(),
          "the small test input fits the local kernel buffer");
}

std::string peer_drain(int fd) {
  std::string result;
  std::array<char, 2048> bytes{};
  for (;;) {
    const auto count = ::recv(fd, bytes.data(), bytes.size(), 0);
    if (count > 0) {
      result.append(bytes.data(), static_cast<std::size_t>(count));
      continue;
    }
    if (count < 0 && errno == EINTR)
      continue;
    require(count == 0 || (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)),
            "peer drain ends at EOF or would-block");
    return result;
  }
}

void tick(PollConnection& connection) {
  std::array<PollConnection*, 1> connections{&connection};
  static_cast<void>(poll_once(connections, 20));
}

}

int main(int argc, char** argv) {
  LabExerciseChecks checks(argc, argv);
  checks.run("output_empty", [] {
    OutboundBuffer output(4);
    require(output.front().empty() && output.pending_bytes() == 0, "initial output is empty");
    require(output.try_enqueue(""), "empty enqueue succeeds without a node");
    output.consume(0);
  });
  checks.run("output_partial", [] {
    OutboundBuffer output(10);
    require(output.try_enqueue("abc") && output.try_enqueue("def"), "enqueue two ordered chunks");
    output.consume(2);
    require(output.front() == "c" && output.pending_bytes() == 4, "partial send advances offset");
    output.consume(2);
    require(output.front() == "ef" && output.pending_bytes() == 2, "consume can cross chunks");
    output.consume(2);
    require(output.front().empty() && output.pending_bytes() == 0, "fully sent chunks disappear");
  });
  checks.run("output_limit", [] {
    OutboundBuffer output(4);
    require(output.try_enqueue("abc"), "three bytes accepted");
    require(!output.try_enqueue("de") && output.front() == "abc" && output.pending_bytes() == 3,
            "capacity failure makes no partial insertion");
    output.consume(2);
    require(output.try_enqueue("def") && output.pending_bytes() == 4,
            "sent bytes free output capacity");
    OutboundBuffer zero(0);
    require(zero.try_enqueue("") && !zero.try_enqueue("x"), "zero output capacity is valid");
  });
  checks.run("output_bad_consume", [] {
    OutboundBuffer output(4);
    require(output.try_enqueue("ab"), "setup output");
    bool caught = false;
    try {
      output.consume(3);
    } catch (const std::out_of_range&) {
      caught = true;
    }
    require(caught && output.front() == "ab" && output.pending_bytes() == 2,
            "invalid consumption leaves state unchanged");
  });
  checks.run("interest", [] {
    LocalPair pair;
    PollConnection connection(pair.server(), 4, 4);
    require(connection.wanted_events() == POLLIN, "idle connection asks only for reads");
    require(connection.queue_output("ab"), "small output accepted");
    require(connection.wanted_events() == (POLLIN | POLLOUT), "pending output enables writes");
    connection.pause_reads(true);
    require(connection.wanted_events() == POLLOUT, "backpressure pauses reads, preserves writes");
    connection.pause_reads(false);
    require(connection.wanted_events() == (POLLIN | POLLOUT), "capacity restoration resumes reads");
  });
  checks.run("zero_input", [] {
    LocalPair pair;
    bool caught = false;
    try {
      PollConnection connection(pair.server(), 0, 4);
    } catch (const std::invalid_argument&) {
      caught = true;
    }
    require(caught, "zero input limit cannot make progress");
  });
  checks.run("read_limit", [] {
    LocalPair pair;
    PollConnection connection(pair.server(), 4, 4);
    peer_write(pair.peer(), "abcdef");
    tick(connection);
    require((connection.wanted_events() & POLLIN) == 0, "full input disables read subscription");
    require(connection.take_received() == "abcd", "never read beyond configured input capacity");
    require((connection.wanted_events() & POLLIN) != 0, "draining input restores reads");
    tick(connection);
    require(connection.take_received() == "ef", "unread suffix remains in the kernel");
  });
  checks.run("would_block", [] {
    LocalPair pair;
    PollConnection connection(pair.server(), 16, 16);
    // 即使给出过时 readiness，空 socket 的 recv EAGAIN 也不是失败。
    connection.dispatch(POLLIN);
    require(!connection.failed() && !connection.read_eof() && connection.take_received().empty(),
            "would-block preserves an open connection");
    peer_write(pair.peer(), "ok");
    tick(connection);
    require(connection.take_received() == "ok", "later readiness still works");
  });
  checks.run("pause_and_resume", [] {
    LocalPair pair;
    PollConnection connection(pair.server(), 16, 16);
    connection.pause_reads(true);
    peer_write(pair.peer(), "in");
    require(connection.queue_output("out"), "queue response while paused");
    tick(connection);
    require(peer_drain(pair.peer()) == "out" && connection.take_received().empty(),
            "paused reads do not block independent writes");
    connection.pause_reads(false);
    tick(connection);
    require(connection.take_received() == "in", "resume receives preserved input");
  });
  checks.run("half_close", [] {
    LocalPair pair;
    PollConnection connection(pair.server(), 16, 16);
    peer_write(pair.peer(), "request");
    require(::shutdown(pair.peer(), SHUT_WR) == 0, "peer half-closes its sending direction");
    tick(connection);
    require(connection.read_eof() && !connection.done(),
            "EOF preserves received input for business");
    require(connection.take_received() == "request", "data before EOF remains available");
    require(connection.queue_output("reply"), "read EOF still permits a final reply");
    require(!connection.done(), "pending reply prevents premature close");
    tick(connection);
    require(peer_drain(pair.peer()) == "reply" && connection.done(),
            "drain writes before reclaiming fd");
  });
  checks.run("partial_send", [] {
    LocalPair pair;
    const int small_buffer = 4096;
    require(::setsockopt(pair.server(), SOL_SOCKET, SO_SNDBUF, &small_buffer,
                         sizeof(small_buffer)) == 0,
            "bound the kernel send buffer for a local test");
    const std::string payload(64 * 1024, 'q');
    PollConnection connection(pair.server(), 16, payload.size());
    require(connection.queue_output(payload), "queue larger than the kernel send buffer");
    connection.dispatch(POLLOUT);
    auto received = peer_drain(pair.peer());
    require(!received.empty() && received.size() < payload.size() && !connection.failed(),
            "actual partial send and EAGAIN retain the unsent suffix");
    require((connection.wanted_events() & POLLOUT) != 0, "unsent suffix keeps write subscription");
    for (int turn = 0; turn < 100 && received.size() < payload.size(); ++turn) {
      tick(connection);
      received += peer_drain(pair.peer());
    }
    require(received == payload, "repeated write readiness sends each byte exactly once");
    require((connection.wanted_events() & POLLOUT) == 0,
            "finished output disables write subscription");
  });
  checks.run("two_connections", [] {
    LocalPair slow;
    LocalPair fast;
    PollConnection slow_connection(slow.server(), 16, 16);
    PollConnection fast_connection(fast.server(), 16, 16);
    slow_connection.pause_reads(true);
    peer_write(slow.peer(), "held");
    peer_write(fast.peer(), "fast");
    std::array<PollConnection*, 2> connections{&slow_connection, &fast_connection};
    require(poll_once(connections, 20) >= 1, "poll dispatches an active connection");
    require(fast_connection.take_received() == "fast" && slow_connection.take_received().empty(),
            "a paused connection does not prevent another from progressing");
  });
  checks.run("fatal_event", [] {
    LocalPair pair;
    PollConnection connection(pair.server(), 16, 16);
    connection.dispatch(POLLNVAL);
    require(connection.failed() && connection.done() && connection.wanted_events() == 0,
            "invalid-fd event retires connection state");
    require(!connection.queue_output("x"), "failed state rejects future output");
    std::array<PollConnection*, 1> connections{&connection};
    require(poll_once(connections, 0) == 0, "retired connections are skipped");
  });
  return checks.finish();
}
