#include "observer.cpp"

#include <stdexcept>
#include <type_traits>
#include <utility>
#include <atomic>
#include <thread>
#include <vector>

void require(bool condition, const char* message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

struct Probe {
  static inline int alive = 0;
  static inline int constructed = 0;
  static inline int destroyed = 0;

  explicit Probe(int v) : value(v) {
    ++alive;
    ++constructed;
  }

  ~Probe() {
    --alive;
    ++destroyed;
  }

  int value;
};

struct SharedProbe {
  static inline int alive = 0;
  static inline int destroyed = 0;

  explicit SharedProbe(int v) : value(v) {
    ++alive;
  }

  ~SharedProbe() {
    --alive;
    ++destroyed;
  }

  int value;
};

template <typename Pointer>
bool is_truthy(const Pointer& pointer) {
  if (pointer) {
    return true;
  }
  return false;
}

int main() {
  static_assert(!std::is_copy_constructible_v<UniquePtr<Probe>>);
  static_assert(!std::is_copy_assignable_v<UniquePtr<Probe>>);
  static_assert(std::is_move_constructible_v<UniquePtr<Probe>>);
  static_assert(std::is_move_assignable_v<UniquePtr<Probe>>);
  static_assert(std::is_same_v<decltype(std::declval<UniquePtr<Probe>&>() =
                                            std::declval<UniquePtr<Probe>&&>()),
                               UniquePtr<Probe>&>);

  require(Probe::alive == 0, "Probe starts with no live instances");
  require(Probe::constructed == 0, "Probe construction count starts at zero");
  require(Probe::destroyed == 0, "Probe destruction count starts at zero");

  {
    auto first = UniquePtr<Probe>::make(10);
    require(first.get() != nullptr, "make creates an owned object");
    require(first.get()->value == 10, "make forwards constructor arguments");
    require(Probe::alive == 1, "make creates exactly one live object");

    auto second = std::move(first);
    require(first.get() == nullptr, "move construction empties the source");
    require(second.get() != nullptr, "move construction transfers ownership");
    require(second.get()->value == 10, "moved owner retains the object");
    require(Probe::alive == 1, "move construction does not duplicate the object");

    auto destination = UniquePtr<Probe>::make(20);
    require(Probe::alive == 2, "second make creates one more object");
    UniquePtr<Probe>& assigned = (destination = std::move(second));
    require(&assigned == &destination, "move assignment returns the destination");
    require(second.get() == nullptr, "move assignment empties the source");
    require(destination.get() != nullptr, "move assignment transfers ownership");
    require(destination.get()->value == 10, "move assignment replaces the old value");
    require(Probe::alive == 1, "move assignment releases the destination's old object");
    require(Probe::destroyed == 1, "replacing destination destroys value 20");

    Probe* raw = destination.release();
    require(destination.get() == nullptr, "release empties the owner");
    require(raw != nullptr && raw->value == 10, "release returns the owned pointer");
    require(Probe::alive == 1, "release transfers responsibility to this test");
    delete raw;
    require(Probe::alive == 0, "released object remains manually deletable");
    require(Probe::destroyed == 2, "manual delete destroys released object once");

    auto resettable = UniquePtr<Probe>::make(30);
    require(Probe::alive == 1, "reset test object is alive");
    resettable.reset();
    require(resettable.get() == nullptr, "reset empties the owner");
    require(Probe::alive == 0, "reset destroys the owned object");
    require(Probe::destroyed == 3, "reset destroys the object once");
  }

  require(Probe::alive == 0, "all objects are gone at end of test");
  require(Probe::constructed == 3, "all three Probe objects were constructed");
  require(Probe::destroyed == 3, "all three Probe objects were destroyed once");

  require(SharedProbe::alive == 0, "SharedProbe starts with no live instances");
  require(SharedProbe::destroyed == 0, "SharedProbe destruction count starts at zero");
  {
    SharedPtr<SharedProbe> empty;
    require(!is_truthy(empty), "default SharedPtr converts to false");

    auto owner = SharedPtr<SharedProbe>::make(42);
    require(is_truthy(owner), "non-empty SharedPtr converts to true");
    require(owner.get()->value == 42, "SharedPtr accesses its managed object");
    require(owner.use_count() == 1, "first SharedPtr is the only owner");

    {
      auto copy = owner;
      require(is_truthy(copy), "a copied SharedPtr converts to true");
      require(owner.use_count() == 2, "copy shares the same strong count");
    }
    require(owner.use_count() == 1, "destroying a copy decrements the strong count");

    auto moved = std::move(owner);
    require(!is_truthy(owner), "moved-from SharedPtr converts to false");
    require(is_truthy(moved), "move destination converts to true");
    require(moved.use_count() == 1, "move transfers without changing the count");
  }
  require(SharedProbe::alive == 0, "last SharedPtr destroys the managed object");
  require(SharedProbe::destroyed == 1, "SharedProbe is destroyed exactly once");

  {
    auto move_target = SharedPtr<SharedProbe>::make(70);
    auto move_source = SharedPtr<SharedProbe>::make(80);
    SharedPtr<SharedProbe>& assigned = (move_target = std::move(move_source));
    require(&assigned == &move_target, "move assignment returns the destination");
    require(!is_truthy(move_source), "move assignment empties its source");
    require(is_truthy(move_target), "move assignment transfers ownership to destination");
    require(move_target.get()->value == 80, "move assignment transfers the source object");
    require(move_target.use_count() == 1, "move assignment does not increment the count");
    require(SharedProbe::alive == 1, "move assignment releases the destination's old object");
    require(SharedProbe::destroyed == 2, "move assignment destroys the replaced object once");
  }
  require(SharedProbe::alive == 0, "move-assigned owner destroys its object at final release");
  require(SharedProbe::destroyed == 3, "move-assigned object is destroyed exactly once");

  {
    auto copy_target = SharedPtr<SharedProbe>::make(50);
    auto copy_source = SharedPtr<SharedProbe>::make(60);
    SharedPtr<SharedProbe>& assigned = (copy_target = copy_source);
    require(&assigned == &copy_target, "copy assignment returns the destination");
    require(copy_target.get() == copy_source.get(), "copy assignment shares the source object");
    require(copy_target.use_count() == 2, "copy assignment increments the shared count");
    require(SharedProbe::alive == 1, "copy assignment releases the destination's old object");
    require(SharedProbe::destroyed == 4, "copy assignment destroys the replaced object once");
  }
  require(SharedProbe::alive == 0, "copy-assigned owners destroy their object at final release");
  require(SharedProbe::destroyed == 5, "copy-assigned object is destroyed exactly once");

  {
    auto owner = SharedPtr<SharedProbe>::make(90);
    WeakPtr<SharedProbe> observer(owner);
    require(!observer.expired(), "WeakPtr observes a live object");
    require(observer.use_count() == 1, "WeakPtr does not increase the strong count");

    {
      auto locked = observer.lock();
      require(is_truthy(locked), "lock obtains a SharedPtr while the object is alive");
      require(locked.get()->value == 90, "lock returns access to the original object");
      require(owner.use_count() == 2, "lock adds exactly one strong owner");
    }
    require(owner.use_count() == 1, "destroying the locked owner releases its strong count");

    owner = SharedPtr<SharedProbe>{};
    require(observer.expired(), "WeakPtr expires after the last SharedPtr is released");
    require(!is_truthy(observer.lock()), "lock returns empty after object destruction");
    require(SharedProbe::alive == 0, "expired observer does not keep the object alive");
    require(SharedProbe::destroyed == 6, "object is destroyed when strong count reaches zero");
  }

  const int destroyed_before_race = SharedProbe::destroyed;
  {
    constexpr int worker_count = 4;
    constexpr int attempts_after_release = 1000;
    auto owner = SharedPtr<SharedProbe>::make(1234);
    WeakPtr<SharedProbe> observer(owner);
    std::atomic<int> ready{0};
    std::atomic<int> successful_locks{0};
    std::atomic<int> invalid_values{0};
    std::atomic<bool> start{false};
    std::atomic<bool> owner_released{false};
    std::vector<std::thread> workers;
    workers.reserve(worker_count);

    auto try_lock_and_check = [&] {
      auto locked = observer.lock();
      if (locked) {
        if (locked.get()->value != 1234) {
          invalid_values.fetch_add(1);
        }
        successful_locks.fetch_add(1);
      }
    };

    for (int i = 0; i < worker_count; ++i) {
      workers.emplace_back([&] {
        ready.fetch_add(1);
        while (!start.load())
          std::this_thread::yield();
        while (!owner_released.load()) {
          try_lock_and_check();
          std::this_thread::yield();
        }
        for (int j = 0; j < attempts_after_release; ++j) {
          try_lock_and_check();
        }
      });
    }

    while (ready.load() != worker_count)
      std::this_thread::yield();
    start.store(true);
    while (successful_locks.load() < worker_count)
      std::this_thread::yield();

    owner = SharedPtr<SharedProbe>{};
    owner_released.store(true);
    for (auto& worker : workers)
      worker.join();

    require(invalid_values.load() == 0, "concurrent lock never accesses an invalid object");
    require(observer.expired(), "observer expires after the last shared owner is released");
    require(!observer.lock(), "concurrent lock cannot resurrect an expired object");
    require(SharedProbe::alive == 0, "racing locks do not keep the object alive");
  }
  require(SharedProbe::destroyed == destroyed_before_race + 1,
          "racing locks still destroy the object exactly once");
}
