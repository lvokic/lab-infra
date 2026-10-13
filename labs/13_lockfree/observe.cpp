#include "lab_check.hpp"
#include "lab_exercise_checks.hpp"
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <latch>
#include <thread>

namespace {

void traits() {
  std::atomic<std::uint64_t> counter{0};
  std::atomic<std::size_t> position{0};
  std::atomic<int*> pointer{nullptr};
  std::cout << "[traits] uint64_t=" << counter.is_lock_free()
            << " size_t=" << position.is_lock_free() << " pointer=" << pointer.is_lock_free()
            << " uint64_t_always=" << decltype(counter)::is_always_lock_free << '\n';
  std::cout << "These describe atomic operations, not the whole container or its allocator.\n";
}

void cas() {
  std::atomic<int> state{10};
  int expected = state.load(std::memory_order_relaxed);
  std::jthread other([&] {
    state.store(12, std::memory_order_relaxed);
  });
  other.join();
  const bool first = state.compare_exchange_strong(expected, 20, std::memory_order_relaxed);
  const int after_failure = expected;
  const bool second = state.compare_exchange_strong(expected, 20, std::memory_order_relaxed);
  require(!first && after_failure == 12 && second && state.load() == 20,
          "failed CAS refreshes expected; the next attempt can succeed");
  std::cout << "[cas] first_success=" << first << " expected_after_failure=" << after_failure
            << " second_success=" << second << " final=" << state.load() << '\n';
  std::cout << "For a computed desired value, reconsider the computation after expected changes.\n";
}

void progress() {
  std::atomic_flag gate = ATOMIC_FLAG_INIT;
  std::latch held(1), release_owner(1);
  std::jthread owner([&] {
    gate.test_and_set(std::memory_order_acquire);
    held.count_down();
    release_owner.wait();
    gate.clear(std::memory_order_release);
  });
  held.wait();
  const bool busy = gate.test_and_set(std::memory_order_acquire);
  release_owner.count_down();
  owner.join();
  const bool busy_after_release = gate.test_and_set(std::memory_order_acquire);
  gate.clear(std::memory_order_release);
  require(busy && !busy_after_release, "a held flag prevents another owner from acquiring it");
  std::cout << "[progress] busy_while_held=" << busy << " busy_after_release=" << busy_after_release
            << '\n';
  std::cout << "A spinlock acquisition loop depends on the owner clearing the flag.\n"
            << "An atomic_flag operation is lock-free; a lock built from it is still a lock.\n";
}

constexpr std::uint64_t tagged(std::uint64_t id, std::uint64_t version) {
  return (version << 8) | id;
}

void aba() {
  // 只有整数 ID 和有限次数版本变化；不释放节点、不解引用旧指针。
  std::atomic<int> id{0};
  int stale_id = id.load();
  id.store(1);
  id.store(0);
  const bool plain_accepted = id.compare_exchange_strong(stale_id, 2);
  std::atomic<std::uint64_t> versioned{tagged(0, 0)};
  auto stale_pair = versioned.load();
  versioned.store(tagged(1, 1));
  versioned.store(tagged(0, 2));
  const bool tagged_accepted = versioned.compare_exchange_strong(stale_pair, tagged(2, 1));
  require(plain_accepted && !tagged_accepted && stale_pair == tagged(0, 2),
          "equal IDs can hide a change; the version distinguishes this finite example");
  std::cout << "[aba] history=A->B->A plain_CAS=" << plain_accepted
            << " versioned_CAS=" << tagged_accepted << " observed_version=" << (stale_pair >> 8)
            << '\n';
  std::cout << "A version tag does not itself keep a node alive; finite tags can also wrap.\n";
}

}  // namespace

int main(int argc, char** argv) {
  LabExerciseChecks observations(argc, argv);
  observations.run("traits", traits);
  observations.run("cas", cas);
  observations.run("progress", progress);
  observations.run("aba", aba);
  return observations.finish();
}
