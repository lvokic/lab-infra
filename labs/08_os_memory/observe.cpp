#if defined(__linux__) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif

#include "lab_check.hpp"
#include "mapped_region.hpp"
#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <exception>
#include <fstream>
#include <iostream>
#include <latch>
#include <memory>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <vector>
#include <sys/resource.h>

#if defined(__linux__)
#include <dlfcn.h>
#include <filesystem>
#include <link.h>
#include <pthread.h>
#endif

// 稳定的符号名称：用 nm/readelf 对照 .o、链接后的 ELF 与运行时地址。
extern "C" {
int memory_observe_initialized = 42;
int memory_observe_zero_initialized;
extern const char memory_observe_readonly[] = "read-only global bytes";
thread_local int memory_observe_tls = 7;

void memory_observe_marker() {}
}

namespace {
struct Snapshot {
  std::chrono::steady_clock::time_point time;
  long minor;
  long major;
};

Snapshot snapshot() {
  rusage usage{};
  if (::getrusage(RUSAGE_SELF, &usage) != 0)
    throw std::system_error(errno, std::generic_category(), "getrusage");
  return {std::chrono::steady_clock::now(), usage.ru_minflt, usage.ru_majflt};
}

void report(const char* stage, Snapshot before, Snapshot after) {
  std::cout
      << '[' << stage << "] elapsed_us="
      << std::chrono::duration_cast<std::chrono::microseconds>(after.time - before.time).count()
      << " minor_fault_delta=" << after.minor - before.minor
      << " major_fault_delta=" << after.major - before.major << '\n';
}

// 保留原有基线：这两个 memset 写全部字节，练习的 TODO 则只写页首。
void observe_faults() {
  int main_local = 0;
  auto heap_value = std::make_unique<int>(42);
  std::jthread worker([&] {
    int worker_local = 0;
    std::cout << "[thread] stack_local=" << &worker_local << " shared_heap=" << heap_value.get()
              << " value=" << *heap_value << '\n';
  });
  worker.join();
  std::cout << "[main] stack_local=" << &main_local << " shared_heap=" << heap_value.get() << '\n';
  const auto page = system_page_size();
  constexpr std::size_t pages = 256;
  require(page <= static_cast<std::size_t>(-1) / pages, "mapping size fits size_t");
  const auto start = snapshot();
  MappedRegion region(page * pages);
  const auto mapped = snapshot();
  // 观察程序使用库函数写整个区域。练习独立实现“每页只写一字节”。
  std::memset(region.bytes().data(), 1, region.bytes().size());
  const auto first = snapshot();
  std::memset(region.bytes().data(), 2, region.bytes().size());
  const auto second = snapshot();
  std::uint64_t checksum = 0;
  for (std::size_t offset = 0; offset < region.bytes().size(); offset += page)
    checksum += std::to_integer<unsigned int>(region.bytes()[offset]);
  require(checksum == pages * 2, "second pass writes are observable");
  std::cout << "[mapping] page_size=" << page << " virtual_bytes=" << region.bytes().size()
            << " bytes_written_per_pass=" << region.bytes().size() << " checksum=" << checksum
            << '\n';
  report("mmap", start, mapped);
  report("first-write", mapped, first);
  report("second-write", first, second);
  std::cout << "Fault counters cover the whole process; timing is not a TLB measurement.\n";
}

#if defined(__linux__)
struct Mapping {
  std::uintptr_t low;
  std::uintptr_t high;
  std::string line;
};

std::vector<Mapping> read_maps() {
  std::ifstream input("/proc/self/maps");
  if (!input)
    throw std::runtime_error("cannot read /proc/self/maps");
  std::vector<Mapping> result;
  for (std::string line; std::getline(input, line);) {
    const auto dash = line.find('-');
    const auto space = line.find(' ');
    require(dash != std::string::npos && space != std::string::npos, "maps address range exists");
    result.push_back({static_cast<std::uintptr_t>(std::stoull(line.substr(0, dash), nullptr, 16)),
                      static_cast<std::uintptr_t>(
                          std::stoull(line.substr(dash + 1, space - dash - 1), nullptr, 16)),
                      line});
  }
  require(!result.empty(), "maps contains virtual mappings");
  return result;
}

void print_address(std::string_view label, std::uintptr_t address,
                   const std::vector<Mapping>& maps) {
  std::cout << "  " << label << "=0x" << std::hex << address << std::dec << '\n';
  for (const auto& mapping : maps) {
    if (mapping.low <= address && address < mapping.high) {
      std::cout << "    -> " << mapping.line << '\n';
      return;
    }
  }
  std::cout << "    -> currently unmapped\n";
}

void print_named_mapping(std::string_view name, const std::vector<Mapping>& maps) {
  for (const auto& mapping : maps) {
    if (mapping.line.ends_with(name)) {
      std::cout << "  " << mapping.line << '\n';
      return;
    }
  }
  std::cout << "  " << name << " is absent in this snapshot\n";
}

struct StackRange {
  std::uintptr_t low;
  std::size_t bytes;
  std::size_t guard_bytes;
};

StackRange current_stack() {
  pthread_attr_t attributes;
  const int attribute_error = ::pthread_getattr_np(::pthread_self(), &attributes);
  if (attribute_error != 0)
    throw std::system_error(attribute_error, std::generic_category(), "pthread_getattr_np");
  void* low = nullptr;
  std::size_t bytes = 0;
  std::size_t guard = 0;
  const int stack_error = ::pthread_attr_getstack(&attributes, &low, &bytes);
  const int guard_error = ::pthread_attr_getguardsize(&attributes, &guard);
  ::pthread_attr_destroy(&attributes);
  if (stack_error != 0 || guard_error != 0)
    throw std::system_error(stack_error != 0 ? stack_error : guard_error, std::generic_category(),
                            "pthread stack attributes");
  return {reinterpret_cast<std::uintptr_t>(low), bytes, guard};
}

void print_stack(std::string_view name, StackRange stack, std::uintptr_t local) {
  const auto high = stack.low + stack.bytes;
  std::cout << "[" << name << "] pthread_stack_range=[0x" << std::hex << stack.low << ",0x" << high
            << ") local=0x" << local << std::dec << " bytes=" << stack.bytes
            << " guard_attribute_bytes=" << stack.guard_bytes
            << " local_in_range=" << (stack.low <= local && local < high) << '\n';
}

void pause_for_inspection(std::string_view note) {
  std::cout << "[pause] pid=" << ::getpid() << "; " << note << "; press Enter to continue.\n";
  std::string line;
  std::getline(std::cin, line);
}

void observe_layout(bool pause) {
  std::cout << "\n[layout] pid=" << ::getpid()
            << " executable=" << std::filesystem::read_symlink("/proc/self/exe").string()
            << " page_size=" << system_page_size() << '\n';
  int main_local = 0;
  auto heap = std::make_unique<int>(42);
  MappedRegion anonymous(system_page_size() * 3);
  const auto main_stack = current_stack();

  struct ThreadSample {
    StackRange stack{};
    std::uintptr_t local = 0;
    std::uintptr_t tls = 0;
    std::uintptr_t shared_heap = 0;
    std::exception_ptr error;
  };

  std::array<ThreadSample, 2> samples;
  std::latch arrived(2);
  std::latch release(1);
  std::jthread thread_a;
  std::jthread thread_b;

  // 即使主线程读取 maps 时抛异常，也先释放等待者，再由 jthread 析构 join。
  struct ReleaseOnExit {
    std::latch& gate;

    ~ReleaseOnExit() {
      gate.count_down();
    }
  } release_on_exit{release};

  const auto sample = [&](std::size_t index) {
    int local = 0;
    try {
      samples[index].stack = current_stack();
      samples[index].local = reinterpret_cast<std::uintptr_t>(&local);
      samples[index].tls = reinterpret_cast<std::uintptr_t>(&memory_observe_tls);
      samples[index].shared_heap = reinterpret_cast<std::uintptr_t>(heap.get());
    } catch (...) {
      samples[index].error = std::current_exception();
    }
    arrived.count_down();
    release.wait();  // 保持局部变量存活，让主线程观察两个线程仍存在时的 maps。
  };
  thread_a = std::jthread(sample, 0);
  thread_b = std::jthread(sample, 1);
  arrived.wait();
  for (const auto& entry : samples) {
    if (entry.error)
      std::rethrow_exception(entry.error);
    require(entry.shared_heap == reinterpret_cast<std::uintptr_t>(heap.get()),
            "threads share the same heap object's virtual address");
  }
  const auto maps = read_maps();
  std::cout << "[address -> containing mapping]\n";
  print_address("memory_observe_marker (.text)",
                reinterpret_cast<std::uintptr_t>(&memory_observe_marker), maps);
  print_address("memory_observe_readonly (.rodata)",
                reinterpret_cast<std::uintptr_t>(memory_observe_readonly), maps);
  print_address("memory_observe_initialized (.data)",
                reinterpret_cast<std::uintptr_t>(&memory_observe_initialized), maps);
  print_address("memory_observe_zero_initialized (.bss)",
                reinterpret_cast<std::uintptr_t>(&memory_observe_zero_initialized), maps);
  print_address("new int", reinterpret_cast<std::uintptr_t>(heap.get()), maps);
  print_address("anonymous mmap", reinterpret_cast<std::uintptr_t>(anonymous.bytes().data()), maps);
  print_address("main local", reinterpret_cast<std::uintptr_t>(&main_local), maps);
  print_address("main TLS", reinterpret_cast<std::uintptr_t>(&memory_observe_tls), maps);
  void* libc_malloc = ::dlsym(RTLD_DEFAULT, "malloc");
  require(libc_malloc != nullptr, "dynamic loader finds malloc");
  print_address("loaded malloc implementation", reinterpret_cast<std::uintptr_t>(libc_malloc),
                maps);
  print_stack("main", main_stack, reinterpret_cast<std::uintptr_t>(&main_local));
  for (std::size_t index = 0; index < samples.size(); ++index) {
    const auto label = index == 0 ? "thread-A" : "thread-B";
    print_stack(label, samples[index].stack, samples[index].local);
    print_address("worker local", samples[index].local, maps);
    print_address("worker TLS", samples[index].tls, maps);
    print_address("worker shared heap", samples[index].shared_heap, maps);
  }
  std::cout << "[/proc/self/maps: low address -> high address; workers are still alive]\n"
            << "address-range permissions file-offset device inode pathname\n";
  for (const auto& mapping : maps)
    std::cout << mapping.line << '\n';
  std::cout << "Worker stacks are usually anonymous mappings, without a [stack:tid] label.\n"
            << "pthread stack ranges describe stack bounds; maps shows currently mapped ranges.\n";
  if (pause)
    pause_for_inspection("both workers and the observed objects are still alive");
}

std::uintptr_t program_break() {
  void* result = ::sbrk(0);  // 只查询边界，绝不与 malloc 混用 sbrk/brk 来修改它。
  if (result == reinterpret_cast<void*>(static_cast<std::intptr_t>(-1)))
    throw std::system_error(errno, std::generic_category(), "sbrk(0)");
  return reinterpret_cast<std::uintptr_t>(result);
}

void observe_heap() {
  constexpr std::size_t block_bytes = 4096;
  constexpr std::size_t batches = 4;
  constexpr std::size_t per_batch = 64;
  std::vector<std::unique_ptr<std::byte[]>> allocations;
  allocations.reserve(batches * per_batch);
  std::cout << "\n[heap] hold 256 small allocations, 4096 bytes each\n";
  auto previous = program_break();
  std::cout << "  initial_program_break=0x" << std::hex << previous << std::dec << '\n';
  print_named_mapping("[heap]", read_maps());
  for (std::size_t batch = 0; batch < batches; ++batch) {
    for (std::size_t index = 0; index < per_batch; ++index) {
      auto allocation = std::make_unique_for_overwrite<std::byte[]>(block_bytes);
      allocation[0] = std::byte{1};
      allocations.push_back(std::move(allocation));
    }
    const auto current = program_break();
    std::cout << "  held_bytes=" << allocations.size() * block_bytes
              << " latest_allocation=" << static_cast<void*>(allocations.back().get())
              << " program_break=0x" << std::hex << current << std::dec << " break_direction="
              << (current > previous   ? "higher"
                  : current < previous ? "lower"
                                       : "unchanged")
              << '\n';
    print_named_mapping("[heap]", read_maps());
    previous = current;
  }
  auto large = std::make_unique_for_overwrite<std::byte[]>(2 * 1024 * 1024);
  large[0] = std::byte{2};
  print_address("large new[] (2 MiB)", reinterpret_cast<std::uintptr_t>(large.get()), read_maps());
  allocations.clear();
  std::cout << "[after freeing small allocations] program_break=0x" << std::hex << program_break()
            << std::dec << '\n';
  print_named_mapping("[heap]", read_maps());
  std::cout
      << "malloc may reuse space or use mmap; allocation addresses need not increase.\n"
      << "Freeing allocations need not lower the break. Printing and reading maps also allocate.\n";
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
void sample_stack_frames(std::size_t depth, std::span<std::uintptr_t> addresses,
                         std::vector<Mapping>& deepest_maps) {
  volatile unsigned char frame[4096]{};
  frame[0] = static_cast<unsigned char>(depth);
  addresses[depth] = reinterpret_cast<std::uintptr_t>(&frame[0]);
  if (depth + 1 < addresses.size())
    sample_stack_frames(depth + 1, addresses, deepest_maps);
  else
    deepest_maps = read_maps();
  // 递归返回后仍访问本层活着的对象，避免尾调用把多个层次复用成一个栈帧。
  const unsigned char observed = frame[0];
  require(observed == static_cast<unsigned char>(depth), "recursive frame remains alive");
}

void observe_stack() {
  std::array<std::uintptr_t, 64> addresses{};
  std::vector<Mapping> deepest_maps;
  std::cout << "\n[stack] 64 nested calls, each with a live 4096-byte local array\n";
  std::cout << "[main stack mapping before recursion]\n";
  print_named_mapping("[stack]", read_maps());
  sample_stack_frames(0, addresses, deepest_maps);
  std::size_t lower = 0;
  std::size_t higher = 0;
  for (std::size_t depth = 0; depth < addresses.size(); ++depth) {
    if (depth < 6 || depth + 1 == addresses.size())
      std::cout << "  depth=" << depth << " local_array=0x" << std::hex << addresses[depth]
                << std::dec << '\n';
    else if (depth == 6)
      std::cout << "  ...\n";
    if (depth != 0) {
      if (addresses[depth] < addresses[depth - 1])
        ++lower;
      else if (addresses[depth] > addresses[depth - 1])
        ++higher;
    }
  }
  std::cout << "  transitions_to_lower_address=" << lower
            << " transitions_to_higher_address=" << higher << '\n';
  std::cout << "[main stack mapping while deepest frame was alive]\n";
  print_named_mapping("[stack]", deepest_maps);
  std::cout << "[main stack mapping after returning]\n";
  print_named_mapping("[stack]", read_maps());
  std::cout
      << "Compare separate live frames; C++ does not specify stack direction or local order.\n"
      << "Stack pages can remain mapped after returning; worker stacks have their own bounds.\n";
}

struct LoadedElf {
  std::uintptr_t main_bias = 0;
  bool found_main = false;
};

int print_load_segments(dl_phdr_info* info, std::size_t, void* context) {
  auto& loaded = *static_cast<LoadedElf*>(context);
  const bool is_main = info->dlpi_name[0] == '\0';
  const auto bias = static_cast<std::uintptr_t>(info->dlpi_addr);
  if (is_main) {
    loaded.main_bias = bias;
    loaded.found_main = true;
  }
  std::cout << "[ELF object] " << (is_main ? "main executable" : info->dlpi_name) << " load_bias=0x"
            << std::hex << bias << std::dec << '\n';
  for (std::size_t index = 0; index < info->dlpi_phnum; ++index) {
    const auto& segment = info->dlpi_phdr[index];
    if (segment.p_type != PT_LOAD)
      continue;
    const auto runtime = bias + static_cast<std::uintptr_t>(segment.p_vaddr);
    std::cout << "  LOAD file_offset=0x" << std::hex << segment.p_offset << " ELF_vaddr=0x"
              << segment.p_vaddr << " runtime=[0x" << runtime << ",0x" << runtime + segment.p_memsz
              << ")" << std::dec << " file_bytes=" << segment.p_filesz
              << " memory_bytes=" << segment.p_memsz
              << " permissions=" << ((segment.p_flags & PF_R) ? 'r' : '-')
              << ((segment.p_flags & PF_W) ? 'w' : '-') << ((segment.p_flags & PF_X) ? 'x' : '-')
              << '\n';
  }
  return 0;
}

void observe_elf() {
  ElfW(Ehdr) header{};
  std::ifstream executable("/proc/self/exe", std::ios::binary);
  executable.read(reinterpret_cast<char*>(&header), static_cast<std::streamsize>(sizeof(header)));
  require(executable.good() && std::memcmp(header.e_ident, ELFMAG, SELFMAG) == 0,
          "running executable has a native ELF header");
  std::cout << "\n[elf] pid=" << ::getpid()
            << " path=" << std::filesystem::read_symlink("/proc/self/exe").string() << " ELF_type="
            << (header.e_type == ET_DYN    ? "ET_DYN"
                : header.e_type == ET_EXEC ? "ET_EXEC"
                                           : "other")
            << " ELF_entry=0x" << std::hex << header.e_entry << std::dec << '\n';
  LoadedElf loaded;
  ::dl_iterate_phdr(print_load_segments, &loaded);
  require(loaded.found_main, "loader reports the main executable");
  std::cout << "[main entry at runtime] 0x" << std::hex << loaded.main_bias + header.e_entry
            << std::dec << " (ELF startup entry, not necessarily main)\n";
  const auto print_symbol = [&](std::string_view name, std::uintptr_t address) {
    require(address >= loaded.main_bias, "main symbol can be expressed relative to load bias");
    std::cout << "[main symbol] " << name << " runtime=0x" << std::hex << address
              << " ELF_symbol_value=0x" << address - loaded.main_bias << std::dec << '\n';
  };
  print_symbol("memory_observe_marker", reinterpret_cast<std::uintptr_t>(&memory_observe_marker));
  print_symbol("memory_observe_initialized",
               reinterpret_cast<std::uintptr_t>(&memory_observe_initialized));
  print_symbol("memory_observe_zero_initialized",
               reinterpret_cast<std::uintptr_t>(&memory_observe_zero_initialized));
  print_symbol("memory_observe_readonly",
               reinterpret_cast<std::uintptr_t>(memory_observe_readonly));
  std::cout << "runtime VA = load bias + ELF virtual address, for these main-executable symbols.\n"
            << "LOAD ranges above are unrounded; maps is page-aligned and may split permissions.\n"
            << "This is a snapshot after startup reached main; compare two launches for ASLR.\n";
}

void observe_holes() {
  const auto page = system_page_size();
  void* result =
      ::mmap(nullptr, page * 3, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (result == MAP_FAILED)
    throw std::system_error(errno, std::generic_category(), "mmap for hole observation");
  const auto unmap = [page](std::byte* address) {
    ::munmap(address, page);
  };
  auto* bytes = static_cast<std::byte*>(result);
  // 每页独立管理，解除中间页后不会在析构时误解除后来复用空洞的其他映射。
  std::unique_ptr<std::byte, decltype(unmap)> first(bytes, unmap);
  std::unique_ptr<std::byte, decltype(unmap)> middle(bytes + page, unmap);
  std::unique_ptr<std::byte, decltype(unmap)> last(bytes + page * 2, unmap);
  const auto base = reinterpret_cast<std::uintptr_t>(bytes);
  std::cout << "\n[holes] reserve three contiguous virtual pages\n";
  print_address("first page", base, read_maps());
  if (::munmap(middle.get(), page) != 0)
    throw std::system_error(errno, std::generic_category(), "munmap middle page");
  (void)middle.release();
  const auto split = read_maps();
  print_address("first page after unmap", base, split);
  print_address("middle page after unmap", base + page, split);
  print_address("last page after unmap", base + page * 2, split);
  {
    MappedRegion replacement(page);
    const auto address = reinterpret_cast<std::uintptr_t>(replacement.bytes().data());
    print_address("new one-page mmap", address, read_maps());
    std::cout << "  reused_middle_hole=" << (address == base + page) << '\n';
  }
  std::cout
      << "Unmapped virtual gaps do not themselves reserve RAM; hole reuse is not guaranteed.\n"
      << "Virtual gaps, allocator fragmentation and physical-page fragmentation differ.\n";
}
#endif

void print_help() {
  std::cout << "Usage: memory_observe [MODE] [--pause]\n"
            << "  --faults  original anonymous-mapping and two-write baseline (default)\n"
            << "  --layout  Linux: global/code/heap/mmap/TLS and two live worker stacks\n"
            << "  --elf     Linux: ELF LOAD segments, load bias and runtime symbol addresses\n"
            << "  --heap    Linux: small/large allocations, program break and [heap]\n"
            << "  --stack   Linux: compare live recursive frames and main stack mappings\n"
            << "  --holes   Linux: unmap a middle page and observe virtual-address reuse\n"
            << "  --all     run every observation\n"
            << "  --pause   wait for Enter; --layout keeps both workers and its objects alive\n"
            << "Use a native Debug build to study normal layout; sanitizers change mappings.\n";
}
}  // namespace

// 已完成的观察程序；page_exercises.hpp 仍由你独立实现。
int main(int argc, char** argv) {
  try {
    std::string_view mode = "--faults";
    bool selected = false;
    bool pause = false;
    for (int index = 1; index < argc; ++index) {
      const std::string_view option = argv[index];
      if (option == "--help") {
        print_help();
        return 0;
      }
      if (option == "--pause") {
        pause = true;
      } else {
        require(!selected, "choose one mode, or use --all");
        require(option == "--faults" || option == "--layout" || option == "--elf" ||
                    option == "--heap" || option == "--stack" || option == "--holes" ||
                    option == "--all",
                "unknown mode; use --help");
        mode = option;
        selected = true;
      }
    }
#if defined(__linux__)
    if (mode == "--elf" || mode == "--all")
      observe_elf();
    if (mode == "--layout" || mode == "--all") {
      observe_layout(pause);
      pause = false;
    }
    if (mode == "--heap" || mode == "--all")
      observe_heap();
    if (mode == "--stack" || mode == "--all")
      observe_stack();
    if (mode == "--holes" || mode == "--all")
      observe_holes();
#else
    if (mode != "--faults")
      std::cout << "[SKIP] layout/elf/heap/stack/holes observations require Linux.\n";
#endif
    if (mode == "--faults" || mode == "--all")
      observe_faults();
    if (pause) {
#if defined(__linux__)
      pause_for_inspection("observations have returned; local objects have been released");
#else
      std::cout << "[pause] pid=" << ::getpid() << "; press Enter to exit (workers have joined).\n";
      std::string line;
      std::getline(std::cin, line);
#endif
    }
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
