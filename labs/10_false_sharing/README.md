# L10｜真正共享、false sharing 与布局控制（主线 3h）

先完成 L07 的 atomic 基础与 L09 测量边界。固定两个线程、默认小工作量，重点是计数、布局和解释。
主线 3h 含基础实现；绑定核、perf、一致性流量和大规模测量另计。

## 1. 阅读与模型（30min）

先看 [R10 Cache Memories](../../docs/RESOURCES.md) 的 cache line；再读原作者 [Memory 文档](https://www.akkadia.org/drepper/cpumemory.pdf) §3.3.4 Multi-Processor Support 与 §6.4 Multi-Thread Optimizations 的 false sharing 说明。
结合 [fetch_add](https://eel.is/c++draft/atomics.types.operations) 和 L07 解释：relaxed 增量仍原子，但不发布其他普通变量。
画两个核心修改一个计数器，以及修改不同计数器的区别；false sharing 涉及不同对象位于同一一致性维护行，true sharing 修改同一对象。

## 2. 文件与运行（15min）

| 文件 | 用途 |
|---|---|
| [observe.cpp](observe.cpp) | 已完成：三种实际对象布局、地址、计时与校验 |
| [sharing_exercises.hpp](sharing_exercises.hpp) | TODO：运行时槽位映射、递增、线程运行 |
| [sharing_exercises_test.cpp](sharing_exercises_test.cpp) | 槽位、边界、累计与并发检查 |
| [benchmark.cpp](benchmark.cpp) | 测量你实现的槽位与线程接口；不代替 TODO |

```bash
cmake --preset debug &&
cmake --build --preset debug --target false_sharing_test --parallel 2 &&
./build/debug/false_sharing_test --help
cmake --preset release &&
cmake --build --preset release --target false_sharing_observe --parallel 2 &&
./build/release/false_sharing_observe
```

case 为 slots、invalid、increment、concurrent；all 运行全部。TODO 失败是初始状态，observer 和练习独立。

## 3. 先看布局，再看计时（35min）

observer 创建一个共享 atomic、数组内两个相邻 atomic、两个 alignas(256) 独立对象。
**256 是实验指定间隔，不是硬件 cache line 检测结果。**Linux 尝试 sysconf 的 L1 data cache line 查询，失败/不可用为 -1；macOS 可额外运行 `sysctl -n hw.cachelinesize` 并记录。
公开 L1 值不证明所有层级或一致性结构相同，缺失信息时保留不确定性。

| 字段 | 含义与限制 |
|---|---|
| configured_spacing | 实验结构 sizeof，不是硬件事实 |
| atomic_is_always_lock_free | false 时不假定全程为原生无锁 RMW |
| address_gap | 计数器对象起始地址距离，单位字节 |
| reported_line_start_same | 根据公开行大小计算两起始地址是否在同一行，条件性证据 |
| startup-only | 相同启动/join、零递增的单独观测 |
| ns_including_start_join | 含启动/join，不应自动减 startup 得出精确纯循环时间 |

每线程 20000 次 relaxed fetch_add，3 轮交替次序；join 后检查总和 40000，共享计数器只算一次。
打印样本与中位数，没有速度阈值。默认次数只示范方法，调度/启动成本可能占主导。
相邻对象可能跨行，不能仅凭标签叫 false sharing；间隔 256 字节也不在缺乏硬件资料时保证所有机器均分离。

## 4. 自己实现（70min）

练习不同于 observer 的静态结构：你操作一段 atomic 槽位数组，支持运行时布局。

1. select_counter：shared→0，adjacent→worker，spaced→worker×spacing。spacing 单位是槽，不是字节；先推演 2 workers、spacing=4，通过 slots。
2. 拒绝零 spacing、槽位越界与乘法溢出。run 只允许 1 或 2 workers；启动任何线程前验证所有槽位，错误时不能产生部分更新。通过 invalid。
3. increment：不重置原值；零迭代保留初值；通过 increment。
4. run：验证后启动 worker，全部 join 后求和；共享对象不重复计入总和，未选中槽不变，第二轮继续累计。通过 concurrent。

核心保持 TODO；只需实现 header，入口和检查已提供。建议 jthread 管理生命周期，提前检查保证工作线程不抛异常。
join 后检查；relaxed 不替代 mutex 保护其他复杂状态。测试次数较小，没有性能断言。

```bash
./build/debug/false_sharing_test slots
./build/debug/false_sharing_test concurrent
./build/debug/false_sharing_test all
```

## 5. 验收与复盘（30min）

四项检查通过，画三种槽位图，解释字节间隔的计算；说明 relaxed 原子递增不丢更新，却不能发布普通内存。

使用自己的实现运行预置计时入口：

```bash
cmake --preset release &&
cmake --build --preset release --target false_sharing_benchmark --parallel 2 &&
./build/release/false_sharing_benchmark
```

入口调用 select_counter/run，不提供它们的算法；默认两个线程，每线程 20000 次，
三轮交替次序，另测零迭代启动成本。每轮重新创建槽位，保留单次 run 的实际总数检查。
spaced 使用 32 个 atomic 槽的间隔，并打印实际地址与字节数；仍不代表检测到了硬件行大小。
该计时入口未实现时会明确报 TODO，不加入 CTest。

记录线程数、次数、编译模式、地址、公开行大小、lock-free 状态和各轮时间。
无显著差异也可完成：讨论同核调度、频率、其他任务、样本量、启动成本和硬件结构。
耗时不能独立证明 coherence 流量；未复验目标硬件时不宣称消除所有 false sharing。
扩展前协调共享服务器资源，不进行大压力测试。随后进入 L11 网络 I/O。
