# L08｜地址空间、ELF 装载、匿名映射与首次写入（主线 4h）

完成 L07 后进入本节。这里既有可以直接运行的 observer，也有需要你补全的访问代码。
目标是把申请地址范围、触碰页面、缺页与访问缓存分开解释。主线 4h 包含阅读、观察、基础实现和复盘；大规模扫描、NUMA/huge-page/perf 扩展另计。

## 1. 阅读顺序（50min）

已有材料索引：[R09/R10](../../docs/RESOURCES.md)。按以下顺序读，不必通读教材。

| 材料 | 阅读位置 | 带着什么问题读 |
|---|---|---|
| [OSTEP Address Spaces](https://pages.cs.wisc.edu/~remzi/OSTEP/vm-intro.pdf) | §13.3，Figure 13.3 | 打印出来的指针是虚拟地址还是物理地址？ |
| [OSTEP Paging](https://pages.cs.wisc.edu/~remzi/OSTEP/vm-paging.pdf) | VPN/PFN 与页内偏移的例子 | 连续虚拟页是否要求连续物理页？ |
| [OSTEP TLBs](https://pages.cs.wisc.edu/~remzi/OSTEP/vm-tlbs.pdf) | TLB hit/miss 与上下文切换 | TLB miss 是否一定进入内核？ |
| [mmap 手册](https://man7.org/linux/man-pages/man2/mmap.2.html) | MAP_ANONYMOUS、MAP_PRIVATE、返回值、munmap | 映射成功是否等于所有页已获得可写物理页？ |
| [getrusage 手册](https://man7.org/linux/man-pages/man2/getrusage.2.html) | ru_minflt、ru_majflt、RUSAGE_SELF | 计数覆盖一个循环，还是整个进程？ |

先画一个进程的堆、映射区、主线程栈和工作线程栈。线程共享地址空间，但各有调用栈与寄存器状态；不同栈地址不代表不同进程。
随后用 `--layout` 修正这张示意图：真实地址范围、排列与空洞，以当前进程的映射为准。ELF 装载、增长方向和空洞实验是按需复习的扩展，不要求先看完整门 OS 课程。

## 2. 文件与运行入口（20min）

| 文件 | 状态与用途 |
|---|---|
| [observe.cpp](observe.cpp) | 已完成：首次写入基线；Linux 地址分布、ELF、堆、递归栈和映射空洞实验 |
| [mapped_region.hpp](mapped_region.hpp) | 已完成：mmap/munmap RAII 与 page size 查询 |
| [page_exercises.hpp](page_exercises.hpp) | 你的实现：页偏移、按页写入、按页校验，全部 TODO |
| [page_exercises_test.cpp](page_exercises_test.cpp) | 已完成：边界与真实映射检查 |
| [benchmark.cpp](benchmark.cpp) | 已完成测量入口：调用你实现的按页接口；TODO 未完成时失败 |

在项目根目录运行：

```bash
cmake --preset debug &&
cmake --build --preset debug --target memory_observe memory_access_test --parallel 2 &&
./build/debug/memory_observe
./build/debug/memory_access_test --help
```

检查入口接受 `all` 或 `offsets`、`invalid`、`touch`、`mapping` 单项。尚未实现时检查明确报告 TODO，这是正常初始状态。

`memory_observe` 不调用练习的 TODO，可以先独立探索。无参数仍运行原来的两轮写入基线：

| 选项 | 观察什么 |
|---|---|
| `--faults` | 匿名映射、第一次和第二次完整写入、minor/major fault |
| `--elf` | ELF `PT_LOAD`、装载基址偏移、链接值与实际运行地址 |
| `--layout` | 代码、只读/可写全局数据、堆对象、匿名映射、TLS、两个仍存活的工作线程栈 |
| `--heap` | 多批小分配、program break、`[heap]` 范围，以及一次大分配 |
| `--stack` | 64 层递归中仍存活的局部数组地址、主线程栈映射的变化 |
| `--holes` | 解除中间一页后的虚拟地址空洞，以及后续映射是否复用空洞 |
| `--all` | 顺序运行全部观察 |
| `--help` | 查看选项；`--pause` 可与一个观察模式组合 |

建议用**没有 sanitizer 的 Debug 构建**观察通常的 Linux 布局；ASan/TSan 会增加映射、改变分配方式，ASan 也可能改变栈上对象的位置。

```bash
cmake --preset debug &&
cmake --build --preset debug --target memory_observe --parallel 2 &&
./build/debug/memory_observe --layout
./build/debug/memory_observe --elf
./build/debug/memory_observe --heap
./build/debug/memory_observe --stack
./build/debug/memory_observe --holes
```

## 3. 解释 observer 输出（40min）

### 3.1 原有的两轮写入

1. `thread/main`：shared_heap 相同，局部变量地址不同。worker 输出后先 join，主线程再输出，避免 cout 混杂；地址本身不能告诉你物理页位置。
2. `mapping`：采用本机 page size，映射 256 页；4 KiB 页机器约 1 MiB。virtual_bytes 是映射范围，bytes_written_per_pass 是每轮写入量。
3. `mmap` 阶段没有提前读取或清零整个区域。
4. `first-write/second-write` 都用 memset 写整个区域。最后每页抽样一字节，checksum 应为 512；不符会返回非零。**observer 写全部字节，练习要求每页只写一字节，两者工作量不同。**
5. 记录耗时与 minor/major fault 增量。匿名页首次写入可能产生 minor faults；计数还包含运行库、计时调用等进程活动，不能精确归因于某个 store。

不要求首次一定更慢、fault 必须恰好 256 或第二轮为零。minor fault 不等于磁盘 I/O。
此程序没有测 TLB miss、RSS、带宽或透明大页状态，不能用时间证明这些机制。
Linux/macOS 可运行 POSIX 基线，getrusage 的计数实现要结合本机手册记录。

### 3.2 实际地址空间：`--layout`

程序先打印 PID、可执行文件路径、page size。每个样本地址后紧跟一行包含它的 `/proc/self/maps` 记录，最后打印从低地址到高地址的完整映射。

```text
起始地址-结束地址  权限  文件偏移  设备  inode  文件或映射名称
```

范围是 `[start, end)`。`r/w/x` 分别表示可读、可写、可执行；最后的 `p/s` 区分私有映射和共享映射，**`p` 不表示同一进程的其他线程不能访问它**。`offset` 是映射对应的文件偏移，不是物理地址。对照材料：[proc_pid_maps](https://man7.org/linux/man-pages/man5/proc_pid_maps.5.html)。

| 打印的样本 | 结合映射看什么 |
|---|---|
| `memory_observe_marker` | 函数通常位于可执行的 `.text` 对应映射 |
| `memory_observe_readonly` | 只读字符数组通常位于 `.rodata` 对应映射 |
| `memory_observe_initialized` | 有非零初值的可写全局对象通常位于 `.data` |
| `memory_observe_zero_initialized` | 零初始化全局对象通常位于 `.bss` |
| `new int`、`anonymous mmap` | 小型动态分配和显式匿名映射是否落在不同区域 |
| `loaded malloc implementation` | 动态链接器解析出的实现地址落在哪个已装载库中 |
| `main local`、两个 `worker local` | 主线程栈、工作线程栈各自在哪个范围 |
| main/worker TLS | `thread_local` 对象在不同线程中有不同地址 |

工作线程先记录地址，再等待主线程完成观察；打印时它们的局部对象还活着。现代 Linux 通常不再把工作线程栈标成 `[stack:tid]`，所以程序用 `pthread_getattr_np` 查询栈范围，再用局部变量地址定位映射。`pthread_stack_range` 描述栈边界，**不代表这个范围内的所有页面此刻都已映射或占据物理内存**；主线程尤其需要与 `[stack]` 对照。对照材料：[pthread_getattr_np](https://man7.org/linux/man-pages/man3/pthread_getattr_np.3.html)。

想从另一个终端查看同一个进程：

```bash
./build/debug/memory_observe --layout --pause
```

根据程序输出的 PID，在另一个终端运行 `cat /proc/那个PID/maps`；两个工作线程此时仍在等待。回到第一个终端按 Enter 才释放它们并退出。其他单项模式的 `--pause` 在函数返回后等待，此时局部实验对象已经释放。

### 3.3 编译、链接、装载：`--elf`

先分清三个阶段：

| 阶段 | 本项目中的文件或信息 | 地址的含义 |
|---|---|---|
| 编译得到 `.o` | `build/debug/CMakeFiles/memory_observe.dir/labs/08_os_memory/observe.cpp.o` | 通常是 ELF `ET_REL`；定义的普通符号值表示相对于所属 section 的偏移，尚未决定最终运行地址 |
| 链接得到可执行文件 | `build/debug/memory_observe`、`memory_observe.link.map` | section 被安排进最终 ELF，普通符号得到链接值；`PT_LOAD` 描述装载范围 |
| 加载并开始运行 | `--elf` 的 load bias、实际符号地址，以及 `--layout` 的 maps | 内核与动态链接器建立进程映射；PIE/共享库的运行地址受装载位置影响 |

`.out`、`.exec` 或没有扩展名都可以是可执行文件名；**扩展名不决定编译阶段或文件类型**。这里 CMake 的 `.o` 是编译产物，`memory_observe` 已经过链接，类型由 ELF 头决定。程序输出的运行时快照发生在启动进入 `main` 之后，不是装载过程每一步的追踪。

在项目根目录依次运行：

```bash
file build/debug/CMakeFiles/memory_observe.dir/labs/08_os_memory/observe.cpp.o
readelf -hW build/debug/CMakeFiles/memory_observe.dir/labs/08_os_memory/observe.cpp.o
readelf -SW build/debug/CMakeFiles/memory_observe.dir/labs/08_os_memory/observe.cpp.o
readelf -sW build/debug/CMakeFiles/memory_observe.dir/labs/08_os_memory/observe.cpp.o | rg 'memory_observe_'

file build/debug/memory_observe
readelf -hW build/debug/memory_observe
readelf -SW build/debug/memory_observe
readelf -lW build/debug/memory_observe
nm -n build/debug/memory_observe | rg 'memory_observe_'
rg -n 'memory_observe_' build/debug/memory_observe.link.map
./build/debug/memory_observe --elf
```

对照材料：[readelf](https://sourceware.org/binutils/docs/binutils/readelf.html)、[dl_iterate_phdr](https://man7.org/linux/man-pages/man3/dl_iterate_phdr.3.html)。CMake 在 Linux 链接这个目标时自动生成 link map，便于看到输入对象与输出 section 的对应关系。

重点检查这条关系，程序已为四个普通的主程序符号打印两边的值：

```text
实际运行虚拟地址 = load_bias + 链接后 ELF 的虚拟地址/普通符号值
```

`ELF_symbol_value` 应与可执行文件的 `nm` 输出一致；不要拿 `.o` 的 section 内偏移直接代入。`ET_DYN` 的主程序通常是 PIE；`ET_EXEC` 的常规非 PIE 程序通常使用固定的代码/数据地址，主程序的 load bias 为零。TLS 符号有不同的寻址规则，这个公式不用于 `memory_observe_tls`。

**Section 与 segment 是两种视角。** `.text/.rodata/.data/.bss` 是链接时的 section；OS 装载依据 program headers 中的 `PT_LOAD` segment，一个 segment 可包含多个 section。`readelf -lW` 的末尾显示它们的对应关系。`.bss` 通常不在文件中保存整块零字节，装载后的内存范围会包含零初始化的空间。`p_paddr` 字段不能用来得知 Linux 用户进程的实际物理页。

程序的 `LOAD` 地址未按 page size 舍入；maps 的范围按页对齐，动态链接器还可能因 RELRO 把权限区间拆开，所以两份输出不要求逐行一一对应。`ELF_entry` 是启动入口，通常是 `_start`，不是 `main` 的地址。

将 `--elf` 连续运行两次，比较**同一可执行文件**的 load bias、运行时地址和 `ELF_symbol_value`。ASLR 通常改变前两者；后者在不重新构建时保持一致。无需修改服务器的 ASLR 配置。

### 3.4 堆为什么看起来向上增长：`--heap`

实验持有四批小分配，每批 64 个 4096 字节对象，总计约 1 MiB。每批打印最近分配地址、program break 与 `[heap]` 的边界，再打印一次 2 MiB 分配所在的映射。

传统的主 arena 可以通过增大 program break 扩展 `[heap]` 的高地址端；它会按批扩展，所以 break 可能先不变、随后增长。程序只调用 `sbrk(0)` 查询边界，不自行改变 allocator 使用的 break。

**动态分配并不全部落在 `[heap]`。** glibc 可以复用已释放的块，也可以为大分配或其他 arena 使用独立的匿名映射；阈值与历史状态会影响选择。因此不要求各个 `new` 地址单调增加，也不要求大分配必定使用 mmap。释放小对象后 allocator 可以保留空间，break 不一定下降。打印和读取 maps 本身也可能申请内存。对照材料：[malloc](https://man7.org/linux/man-pages/man3/malloc.3.html)、[brk/sbrk](https://man7.org/linux/man-pages/man2/brk.2.html)。

### 3.5 栈为什么看起来向下增长：`--stack`

实验递归 64 层，每层有一个仍存活的 4096 字节数组，递归返回后仍读取本层数组以避免尾调用复用栈帧；额外局部数组空间合计约 256 KiB。程序保留深层调用时的 maps，并打印前几层和最深层的地址。

在通常的 Linux x86-64 native 构建中，更深层栈帧的地址降低，`transitions_to_lower_address` 通常为 63；若跨过原有映射边界，主线程 `[stack]` 的低地址端也可能降低。函数返回后，栈帧退出，但 OS 可以保留已经映射的页面，不要求 `[stack]` 立刻缩小。

比较的是**不同调用中同时存活的对象**，不要拿同一函数内两个局部变量的先后声明推断栈增长方向。方向与帧大小是 ABI、编译器和构建方式的现象，C++ 不规定它们。工作线程一般使用有边界和 guard 的预留栈空间；向下使用更多栈帧，不表示会无界扩展它的映射。

### 3.6 地址空洞会不会造成很多内存碎片：`--holes`

实验先映射连续三页，解除中间页，再申请一页观察它是否复用了空洞；程序只打印解除映射后的地址值，保持两侧页面各自的所有权。中间页**可能**被后续 mmap 复用，不要求每次如此；内核也可能合并相邻、属性相同的映射。

区分三件事：

| 现象 | 意义 |
|---|---|
| 虚拟地址空洞 | maps 中相邻映射之间未映射的范围；空洞本身不占对应大小的物理内存 |
| allocator 内部的空闲块、对齐或元数据开销 | 可能形成动态分配的内部/外部碎片；maps 看不到每个 malloc 小块的使用情况 |
| 物理页碎片 | 空闲物理页分散；连续虚拟地址通常可以映射到分散的物理页，申请物理连续的大页则有额外要求 |

所以“代码、堆、库、线程栈分布在不同位置”不会直接等于消耗了这些地址跨度那么多的 RAM。反过来，已经映射但尚未触碰的范围，也不能只靠 maps 判断其物理驻留量。对照材料：[Linux 内存管理概念](https://docs.kernel.org/admin-guide/mm/concepts.html)。

Linux 的六组完成观察可以一起检查，仍不调用 learner TODO：

```bash
ctest --test-dir build/debug -R '^memory_(observe|layout|elf|heap|stack|holes)$' --output-on-failure
```

## 4. 你的代码练习（70min）

先只读 header 的契约，然后自己推演循环。

| 顺序 | 函数 | 自己处理的细节 | 检查 |
|---|---|---|---|
| A | page_offsets | 空区域、整页、末尾半页、无符号溢出 | offsets |
| B | 三个函数的错误路径 | page_size 为零统一抛 invalid_argument | invalid |
| C | touch_pages | 只写页首；其他字节不变；返回次数 | touch |
| D | checksum_pages | 字节转无符号数；只读页首；空区域为零 | touch、mapping |

span 提供范围，不拥有映射。检查用 4 字节页面只是推演模型，不代表 OS 页大小。
真实 mapping case 最后一页只有 3 字节，仍须访问首字节；巨大 size_t 偏移检查只生成 1–2 个偏移，不分配巨大内存。
MappedRegion 的移动已写好，只需解释为什么转移后旧包装不应再 munmap 同一地址。
不要对映射指针调用 delete/free，也不需要写 signal handler 来故意触发崩溃。

```bash
./build/debug/memory_access_test offsets
./build/debug/memory_access_test touch
./build/debug/memory_access_test all
```

## 5. 记录与验收（60min）

记录 OS、编译器、page size、映射字节数、每轮写入量、耗时、fault 增量和 checksum。
画 VA→VPN/offset→TLB/页表→PFN/offset；再画不存在或权限不允许的页的异常处理路径。
解释 syscall、page fault 与线程上下文切换是不同事件；syscall 不一定切到另一线程。

四项检查通过；解释映射与首次触碰的区别，指出计时不能证明的两件事；将 CXL 查询拆成线程等待、地址翻译和数据访问，明确哪些只是模型、哪些在目标服务器测过。

完成 header 后直接测量自己的按页代码，不必自行编写 runner：

```bash
cmake --preset release &&
cmake --build --preset release --target memory_benchmark --parallel 2 &&
./build/release/memory_benchmark
```

该入口映射 256 页，每轮只写 256 个页首字节；不提前清零或预热映射。
记录 mmap、第一次和第二次写入的耗时及进程 fault 增量，最后核对写入次数和 checksum。
它不加入 CTest，也不在 TODO 未完成时退回参考实现。

默认小规模足够学习。扩展前协调共享机器资源并检查空闲内存，避免 swap 压力。完成后进入 L09。
