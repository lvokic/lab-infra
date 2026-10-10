#include "epoll_exercises.hpp"
#include "lab_check.hpp"
#include "lab_exercise_checks.hpp"
#include <algorithm>
#include <array>
#include <fcntl.h>

namespace {

// 完整测试支撑：只创建和关闭本机非阻塞 socket，不实现 epoll 或 drain 算法。
class LocalPair {
public:
  LocalPair() {
    require(::socketpair(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0, sockets_.data()) ==
                0,
            "create nonblocking local socketpair");
  }

  LocalPair(const LocalPair&) = delete;
  LocalPair& operator=(const LocalPair&) = delete;

  ~LocalPair() {
    for (int fd : sockets_)
      if (fd >= 0)
        ::close(fd);
  }

  int server() const {
    return sockets_[0];
  }

  int peer() const {
    return sockets_[1];
  }

  void close_peer() {
    ::close(sockets_[1]);
    sockets_[1] = -1;
  }

private:
  std::array<int, 2> sockets_{-1, -1};
};

template <typename Exception, typename Action>
void expect_exception(Action action, std::string_view message) {
  bool caught = false;
  try {
    action();
  } catch (const Exception&) {
    caught = true;
  }
  require(caught, message);
}

void peer_write(int fd, std::string_view bytes) {
  const auto count = ::send(fd, bytes.data(), bytes.size(), MSG_NOSIGNAL);
  require(count >= 0 && static_cast<std::size_t>(count) == bytes.size(),
          "small local test input fits the socket buffer");
}

std::string read_exact_small(int fd, std::size_t count) {
  std::array<char, 64> bytes{};
  require(count <= bytes.size(), "small raw read fits helper buffer");
  const auto received = ::recv(fd, bytes.data(), count, 0);
  require(received >= 0 && static_cast<std::size_t>(received) == count,
          "receive the requested small prefix");
  return std::string(bytes.data(), count);
}

// 对端测试支撑使用原始 recv；不会代替待练习函数处理服务端输入。
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
            "test peer stops draining at EOF or would-block");
    return result;
  }
}

bool includes(const std::vector<EpollReady>& ready, int fd, std::uint32_t events) {
  return std::any_of(ready.begin(), ready.end(), [&](const auto& entry) {
    return entry.fd == fd && (entry.events & events) == events;
  });
}

void expect_ready(EpollSet& set, int fd, std::uint32_t events) {
  const auto ready = set.wait(20);
  require(ready.size() == 1 && includes(ready, fd, events), "one expected socket is ready");
}

}

int main(int argc, char** argv) {
  LabExerciseChecks checks(argc, argv);
  checks.run("instance", [] {
    LocalPair pair;
    int owned = -1;
    {
      EpollSet set;
      owned = set.native_handle();
      const int flags = ::fcntl(owned, F_GETFD);
      require(owned >= 0 && flags >= 0 && (flags & FD_CLOEXEC) != 0,
              "epoll instance owns a close-on-exec fd");
      set.add(pair.server(), EPOLLIN);
    }
    require(::fcntl(owned, F_GETFD) == -1 && errno == EBADF, "epoll fd closes on scope exit");
    require(::fcntl(pair.server(), F_GETFD) >= 0, "watched socket is borrowed and stays open");
  });
  checks.run("empty_wait", [] {
    EpollSet set;
    require(set.size() == 0 && !set.contains(-1) && set.wait(0).empty(), "initial set is empty");
  });
  checks.run("validation", [] {
    LocalPair pair;
    EpollSet set;
    expect_exception<std::invalid_argument>(
        [&] {
          set.add(-1, EPOLLIN);
        },
        "reject negative fd");
    expect_exception<std::invalid_argument>(
        [&] {
          set.modify(-1, EPOLLIN);
        },
        "reject negative modify fd");
    expect_exception<std::invalid_argument>(
        [&] {
          set.remove(-1);
        },
        "reject negative remove fd");
    expect_exception<std::invalid_argument>(
        [&] {
          set.add(pair.server(), EPOLLIN | EPOLLET);
        },
        "trigger flag belongs to trigger argument");
    expect_exception<std::invalid_argument>(
        [&] {
          set.add(pair.server(), EPOLLIN, static_cast<EpollTrigger>(99));
        },
        "reject invalid mode");
    expect_exception<std::invalid_argument>(
        [&] {
          static_cast<void>(set.wait(-1));
        },
        "reject negative timeout");
    expect_exception<std::invalid_argument>(
        [&] {
          static_cast<void>(set.wait(0, 0));
        },
        "reject zero event buffer");
    expect_exception<std::invalid_argument>(
        [&] {
          static_cast<void>(set.wait(0, 65));
        },
        "bound event buffer size");
    require(set.size() == 0 && set.wait(0, 64).empty(),
            "invalid arguments never add interest; maximum allowed buffer is valid");
    set.add(pair.server(), EPOLLIN);
    expect_exception<std::invalid_argument>(
        [&] {
          set.modify(pair.server(), EPOLLONESHOT);
        },
        "invalid modify flags rejected");
    peer_write(pair.peer(), "ok");
    expect_ready(set, pair.server(), EPOLLIN);
  });
  checks.run("registry", [] {
    LocalPair pair;
    EpollSet set;
    set.add(pair.server(), EPOLLIN);
    require(set.size() == 1 && set.contains(pair.server()), "add records fd");
    expect_exception<std::invalid_argument>(
        [&] {
          set.add(pair.server(), EPOLLIN);
        },
        "duplicate add rejected");
    expect_exception<std::out_of_range>(
        [&] {
          set.modify(pair.peer(), EPOLLIN);
        },
        "modify requires registration");
    expect_exception<std::out_of_range>(
        [&] {
          set.remove(pair.peer());
        },
        "remove requires registration");
    set.modify(pair.server(), 0);
    require(set.size() == 1 && set.wait(0).empty(), "zero interest is allowed");
    set.remove(pair.server());
    require(set.size() == 0 && !set.contains(pair.server()), "remove updates registry");
    require(::fcntl(pair.server(), F_GETFD) >= 0, "remove does not close borrowed fd");
    set.add(pair.server(), EPOLLIN);
    require(set.size() == 1, "removed socket may be added again");
    peer_write(pair.peer(), "ab");
    expect_ready(set, pair.server(), EPOLLIN);
    set.modify(pair.server(), EPOLLIN, EpollTrigger::edge);
    expect_ready(set, pair.server(), EPOLLIN);
    require(read_exact_small(pair.server(), 1) == "a" && set.wait(0).empty(),
            "MOD can switch an existing registration to edge-triggered mode");
  });
  checks.run("ctl_failure", [] {
    EpollSet set;
    // 本地 /dev/null 不支持 epoll。由测试支撑关闭该 fd。
    const int regular = ::open("/dev/null", O_RDONLY | O_CLOEXEC);
    require(regular >= 0, "open local /dev/null");
    bool caught = false;
    try {
      set.add(regular, EPOLLIN);
    } catch (const std::system_error& error) {
      caught = error.code().value() == EPERM;
    } catch (...) {
      ::close(regular);
      throw;
    }
    ::close(regular);
    require(caught && set.size() == 0 && !set.contains(regular),
            "kernel add failure leaves registry unchanged");
  });
  checks.run("lt_partial", [] {
    LocalPair pair;
    EpollSet set;
    set.add(pair.server(), EPOLLIN, EpollTrigger::level);
    peer_write(pair.peer(), "abcdef");
    expect_ready(set, pair.server(), EPOLLIN);
    require(read_exact_small(pair.server(), 2) == "ab", "read only a prefix");
    expect_ready(set, pair.server(), EPOLLIN);
    require(read_exact_small(pair.server(), 4) == "cdef", "LT reports remaining input");
    require(set.wait(0).empty(), "drained LT socket no longer reports reads");
  });
  checks.run("et_partial", [] {
    LocalPair pair;
    EpollSet set;
    set.add(pair.server(), EPOLLIN, EpollTrigger::edge);
    peer_write(pair.peer(), "abcdef");
    expect_ready(set, pair.server(), EPOLLIN);
    require(read_exact_small(pair.server(), 2) == "ab", "read only a prefix");
    require(set.wait(0).empty(), "one ET event does not repeat solely for unread bytes");
    require(read_exact_small(pair.server(), 4) == "cdef", "no new event does not mean no data");
  });
  checks.run("et_drain", [] {
    LocalPair pair;
    EpollSet set;
    set.add(pair.server(), EPOLLIN, EpollTrigger::edge);
    const std::string input(8192, 'q');
    peer_write(pair.peer(), input);
    expect_ready(set, pair.server(), EPOLLIN);
    const auto batch = epoll_read_available(pair.server(), input.size() + 1);
    require(batch.bytes == input && batch.stop == EpollReadStop::would_block,
            "ET handler drains multiple reads through EAGAIN");
    require(set.wait(0).empty(), "drained input does not create another edge");
  });
  checks.run("et_new_edge", [] {
    LocalPair pair;
    EpollSet set;
    set.add(pair.server(), EPOLLIN, EpollTrigger::edge);
    for (std::string_view text : {"first", "second"}) {
      peer_write(pair.peer(), text);
      expect_ready(set, pair.server(), EPOLLIN);
      const auto batch = epoll_read_available(pair.server(), 64);
      require(batch.bytes == text && batch.stop == EpollReadStop::would_block,
              "new data after draining produces a usable edge");
    }
  });
  checks.run("et_budget_resume", [] {
    LocalPair pair;
    EpollSet set;
    set.add(pair.server(), EPOLLIN, EpollTrigger::edge);
    peer_write(pair.peer(), "abcdef");
    expect_ready(set, pair.server(), EPOLLIN);
    const auto first = epoll_read_available(pair.server(), 2);
    require(first.bytes == "ab" && first.stop == EpollReadStop::limit,
            "bounded ET read stops at the user budget");
    require(set.wait(0).empty(), "remaining input alone does not promise a new edge");
    // 用户态 runnable 队列应安排这个续读；不能无限等下一次 epoll 通知。
    const auto resumed = epoll_read_available(pair.server(), 64);
    require(resumed.bytes == "cdef" && resumed.stop == EpollReadStop::would_block,
            "proactive continuation consumes the retained suffix");
  });
  checks.run("modify_interest", [] {
    LocalPair pair;
    EpollSet set;
    set.add(pair.server(), EPOLLIN);
    require(set.wait(0).empty(), "no input initially");
    set.modify(pair.server(), EPOLLOUT);
    expect_ready(set, pair.server(), EPOLLOUT);
    set.modify(pair.server(), EPOLLIN);
    require(set.size() == 1 && set.wait(0).empty(),
            "MOD replaces interest instead of accumulating");
  });
  checks.run("remove_interest", [] {
    LocalPair pair;
    EpollSet set;
    set.add(pair.server(), EPOLLIN);
    peer_write(pair.peer(), "kept");
    set.remove(pair.server());
    require(set.wait(0).empty(), "removed socket is not reported");
    require(read_exact_small(pair.server(), 4) == "kept",
            "remove does not consume or close socket");
  });
  checks.run("multiple_fds", [] {
    LocalPair first;
    LocalPair second;
    EpollSet set;
    set.add(first.server(), EPOLLIN);
    set.add(second.server(), EPOLLIN);
    peer_write(first.peer(), "a");
    peer_write(second.peer(), "b");
    const auto ready = set.wait(20);
    require(ready.size() == 2 && includes(ready, first.server(), EPOLLIN) &&
                includes(ready, second.server(), EPOLLIN),
            "data.fd identifies both ready sockets without imposing event order");
  });
  checks.run("batch_limit", [] {
    LocalPair first;
    LocalPair second;
    EpollSet set;
    set.add(first.server(), EPOLLIN);
    set.add(second.server(), EPOLLIN);
    peer_write(first.peer(), "a");
    peer_write(second.peer(), "b");
    const auto one = set.wait(20, 1);
    require(one.size() == 1 && (one[0].fd == first.server() || one[0].fd == second.server()) &&
                (one[0].events & EPOLLIN) != 0,
            "return only filled entries within max_events");
    static_cast<void>(read_exact_small(one[0].fd, 1));
    const auto other = set.wait(20, 1);
    require(other.size() == 1 && other[0].fd != one[0].fd &&
                (other[0].fd == first.server() || other[0].fd == second.server()) &&
                (other[0].events & EPOLLIN) != 0,
            "another ready socket remains available for the next batch");
  });
  checks.run("half_close", [] {
    LocalPair pair;
    EpollSet set;
    set.add(pair.server(), EPOLLIN | EPOLLRDHUP, EpollTrigger::edge);
    peer_write(pair.peer(), "request");
    require(::shutdown(pair.peer(), SHUT_WR) == 0, "peer half-closes write direction");
    expect_ready(set, pair.server(), EPOLLIN | EPOLLRDHUP);
    const auto batch = epoll_read_available(pair.server(), 64);
    require(batch.bytes == "request" && batch.stop == EpollReadStop::eof,
            "read the last bytes before reporting EOF");
    const auto reply = epoll_write_available(pair.server(), "reply");
    require(reply.sent == 5 && !reply.would_block && peer_drain(pair.peer()) == "reply",
            "read half-close still permits sending the reply");
  });
  checks.run("read_empty", [] {
    LocalPair pair;
    const auto batch = epoll_read_available(pair.server(), 64);
    require(batch.bytes.empty() && batch.stop == EpollReadStop::would_block,
            "EAGAIN preserves an open socket");
    expect_exception<std::invalid_argument>(
        [&] {
          static_cast<void>(epoll_read_available(pair.server(), 0));
        },
        "zero read budget rejected");
  });
  checks.run("read_limit", [] {
    LocalPair pair;
    peer_write(pair.peer(), "abcdef");
    const auto prefix = epoll_read_available(pair.server(), 4);
    require(prefix.bytes == "abcd" && prefix.stop == EpollReadStop::limit,
            "read never exceeds the remaining budget");
    const auto suffix = epoll_read_available(pair.server(), 4);
    require(suffix.bytes == "ef" && suffix.stop == EpollReadStop::would_block,
            "unread suffix remains in the kernel");
  });
  checks.run("read_eof", [] {
    LocalPair pair;
    pair.close_peer();
    const auto batch = epoll_read_available(pair.server(), 64);
    require(batch.bytes.empty() && batch.stop == EpollReadStop::eof, "recv zero reports EOF");
  });
  checks.run("write_empty", [] {
    const auto progress = epoll_write_available(-1, "");
    require(progress.sent == 0 && !progress.would_block, "empty output performs no syscall");
  });
  checks.run("write_complete", [] {
    LocalPair pair;
    const std::string input("a\0bc", 4);
    const auto progress = epoll_write_available(pair.server(), input);
    require(progress.sent == input.size() && !progress.would_block &&
                peer_drain(pair.peer()) == input,
            "small binary output is sent exactly once");
  });
  checks.run("write_backpressure", [] {
    LocalPair pair;
    const int buffer = 4096;
    require(::setsockopt(pair.server(), SOL_SOCKET, SO_SNDBUF, &buffer, sizeof(buffer)) == 0,
            "bound local send buffer");
    std::string input(64 * 1024, 'a');
    for (std::size_t index = 0; index < input.size(); ++index)
      input[index] = static_cast<char>('a' + index % 26);
    EpollSet set;
    set.add(pair.server(), EPOLLOUT, EpollTrigger::edge);
    expect_ready(set, pair.server(), EPOLLOUT);
    const auto first = epoll_write_available(pair.server(), input);
    require(first.sent > 0 && first.sent < input.size() && first.would_block,
            "partial write reaches actual EAGAIN, retaining the suffix");
    std::size_t sent = first.sent;
    std::string received = peer_drain(pair.peer());
    for (int turn = 0; turn < 100 && sent < input.size(); ++turn) {
      expect_ready(set, pair.server(), EPOLLOUT);
      const auto next = epoll_write_available(pair.server(), std::string_view(input).substr(sent));
      require(next.sent <= input.size() - sent,
              "confirmed byte count is bounded by supplied suffix");
      sent += next.sent;
      received += peer_drain(pair.peer());
    }
    require(sent == input.size() && received == input,
            "write-ready edges deliver the whole suffix");
    set.modify(pair.server(), 0, EpollTrigger::edge);
    require(set.wait(0).empty(), "finished output disables write subscription");
  });
  checks.run("io_errors", [] {
    expect_exception<std::system_error>(
        [&] {
          static_cast<void>(epoll_read_available(-1, 4));
        },
        "bad read fd reports syscall failure");
    expect_exception<std::system_error>(
        [&] {
          static_cast<void>(epoll_write_available(-1, "x"));
        },
        "bad write fd reports syscall failure");
    LocalPair pair;
    pair.close_peer();
    expect_exception<std::system_error>(
        [&] {
          static_cast<void>(epoll_write_available(pair.server(), "x"));
        },
        "closed peer reports write error without terminating via SIGPIPE");
  });
  return checks.finish();
}
