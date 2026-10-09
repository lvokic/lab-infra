# L08｜地址空间、匿名映射与首次写入（主线 4h）

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

## 2. 文件与运行入口（20min）

| 文件 | 状态与用途 |
|---|---|
| [observe.cpp](observe.cpp) | 已完成：地址、匿名映射、两轮写入 |
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

## 3. 解释 observer 输出（40min）

1. `thread/main`：shared_heap 相同，局部变量地址不同。worker 输出后先 join，主线程再输出，避免 cout 混杂；地址本身不能告诉你物理页位置。
2. `mapping`：采用本机 page size，映射 256 页；4 KiB 页机器约 1 MiB。virtual_bytes 是映射范围，bytes_written_per_pass 是每轮写入量。
3. `mmap` 阶段没有提前读取或清零整个区域。
4. `first-write/second-write` 都用 memset 写整个区域。最后每页抽样一字节，checksum 应为 512；不符会返回非零。**observer 写全部字节，练习要求每页只写一字节，两者工作量不同。**
5. 记录耗时与 minor/major fault 增量。匿名页首次写入可能产生 minor faults；计数还包含运行库、计时调用等进程活动，不能精确归因于某个 store。

不要求首次一定更慢、fault 必须恰好 256 或第二轮为零。minor fault 不等于磁盘 I/O。
此程序没有测 TLB miss、RSS、带宽或透明大页状态，不能用时间证明这些机制。
Linux/macOS 可运行 POSIX 基线，getrusage 的计数实现要结合本机手册记录。

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
