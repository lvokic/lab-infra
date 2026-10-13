# L13｜无锁编程：CAS 重试与 SPSC 环形队列（专项 5h）

先完成 L06 有界阻塞队列和 L07 发布协议；结合 L10 理解共享缓存行。当前正在 L09 时，可以先读术语和观察程序，把队列实现放在 L10 后。

这项训练面向量化公司的 C++／系统开发：解释进展保证、写受限并发组件、给出同步关系和边界证据。
[Optiver 官方指南](https://www.optiver.com/join-us/stories/optiver-interview-tips-for-software-engineers/)强调并发、体系结构、内存及设计取舍；它没有把手写无锁容器列为固定必考项。这里把 CAS/SPSC 作为 P1 专项，完整 MPMC 与动态节点回收仍属后续扩展，时间不计入原四周 100h。

## 1. 先理解术语（25min）

| 术语 | 要能解释的保证 |
|---|---|
| blocking | 某线程持有锁时，另一个需要同一把锁的操作可能依赖持锁线程继续运行 |
| lock-free | 在持续执行并发操作的条件下，系统整体持续有操作完成；一个特定线程仍可能一直重试 |
| wait-free | 每个操作都能在有界的自身步骤内完成；这不等于实际墙钟时间上限或调度公平性 |
| CAS | 仅当当前值与 expected 相符时提交 desired；失败会更新 expected，weak 还允许伪失败 |
| linearization point | 为一个成功的并发操作选出可以看作瞬时提交的那一步，以解释它与其他操作的先后 |

不能用“没写 mutex”或“atomic 成员都是无锁的”代替整个算法的进展分析。自旋锁等待持有者释放，仍然有锁。
定义与适用限制见 [Boost.Lockfree Introduction](https://www.boost.org/doc/libs/latest/doc/html/lockfree.html)；原子是否无锁见 [C++ atomic lock-free property](https://eel.is/c++draft/atomics.lockfree)。

## 2. 定点材料（35min）

| 材料 | 只读什么 | 带着什么问题 |
|---|---|---|
| [C++ atomic operations](https://eel.is/c++draft/atomics.types.operations) | compare_exchange 的 expected、weak/strong、成功/失败内存序 | 重试时哪些计算需要重新考虑？ |
| [L07](../07_atomic_publish/README.md) | CAS、G 发布、H 接力 | 普通槽位的读写怎样获得 happens-before？ |
| [Boost SPSC 接口](https://www.boost.org/doc/libs/latest/doc/html/doxygen/classboost_1_1lockfree_1_1spsc__queue.html) | single writer/single reader、push/pop、capacity | 为什么限制角色数量可以简化算法？先读契约，再在完成后比较源码 |
| [Boost ABA/回收说明](https://www.boost.org/doc/libs/latest/doc/html/lockfree/rationale.html) | ABA 的 A→B→A 与节点生命周期 | 比较通过为何不代表中间没有变化？版本号能否阻止旧指针被释放？ |

本实验不依赖 Boost、不安装第三方包。Boost 材料只用于核对术语和接口；不要复制库实现完成 TODO。
这里用构造时预分配的 int 槽位，把线程间同步与动态节点回收分开训练。

## 3. 已完成的四组观察（30min）

```bash
cmake --preset debug &&
cmake --build --preset debug --target lockfree_observe --parallel 2 &&
./build/debug/lockfree_observe all
```

也接受单项名称和 `--help`：

| case | 运行前预测 | 运行后要解释 |
|---|---|---|
| traits | 当前 uint64_t、size_t、指针原子是否无锁？ | 输出描述原子操作，并不覆盖 malloc、回收、日志或整个容器 |
| cas | expected 读到 10 后，另一个线程先把状态改成 12 | 第一次失败把 expected 改成 12，第二次成功；为何不能沿用基于旧快照的所有计算？ |
| progress | owner 持有 atomic_flag，另一方尝试获取 | 该原子操作无锁，基于它写出的自旋锁获取却依赖 owner 释放 |
| aba | 状态经历 A→B→A 后比较旧 A | 整数 ID 的 CAS 成功；带版本的这次比较失败；版本号与安全回收是两件事 |

cas 用 strong 与 join 安排确定性顺序；它演示失败处理，不模拟争用频率。
aba 只使用整数 ID，整个过程不释放节点、不解引用悬空指针。版本号示例只经历两次变化，不证明有限版本永不回绕。
这些观察都不调用学员 TODO，也不包含 SPSC 参考实现。

```bash
ctest --test-dir build/debug -R '^lockfree_observe$' --output-on-failure
```

## 4. 文件与契约

| 文件 | 当前内容 |
|---|---|
| [observe.cpp](observe.cpp) | 四组标准原子操作观察，已完成 |
| [lockfree_exercises.hpp](lockfree_exercises.hpp) | AtomicBudget 与 SpscRing 的成员、接口和四个 TODO |
| [lockfree_exercises_test.cpp](lockfree_exercises_test.cpp) | 16 组独立检查，核心未实现时明确失败 |
| [transfer_driver.hpp](transfer_driver.hpp) | 调用端重试、线程退出、FIFO/checksum 检查，已完成 |
| [benchmark.cpp](benchmark.cpp) | 直接测量你实现的 SPSC；TODO 时返回失败，不替换为答案 |

### A：AtomicBudget（45min）

额度只增加，不释放；构造给定 limit，used 从 0 开始。try_acquire(amount) 一次申请整个 amount：

- 成功必须一次提交增加，额度不足返回 false，失败不改变状态。
- amount 为 0 成功且不改变计数；limit 为 0 时拒绝所有正数。
- 任意交错下 used 不超过 limit；加法不能绕回小值。
- 用 CAS 循环。不要先 fetch_add 超额再回滚，也不要把 load 和 store 当成复合原子操作。

先用 limit=10、amount=4/7/6 推演，再看两个线程同时从同一快照申请的交错。
一次 CAS 成功可以作为成功申请的提交点；失败后重新判断余量与 desired。
这个类仅管理计数，不借计数发布其他字段，因此可以讨论 relaxed 的适用性。为自己的选择写一句理由。

### B：SpscRing 的存储与角色（25min）

capacity 表示能存多少个元素；内部多留一个槽位，避免只靠相同位置无法区分满/空。
容量可以是 1、2、3、7，不要求是 2 的幂。零和 SIZE_MAX 抛 invalid_argument；其余内存分配失败正常传播。

先画 capacity=3 时内部四个槽位，标出读位置、写位置与下一位置，推演填满、取出一个、回绕。
只在构造时分配，之后不扩容。生产者只写写位置，消费者只写读位置，两边都会读取对方的位置。

### C/D：入队、出队与两条同步关系（60min）

try_push 满时返回 false；try_pop 空时返回 nullopt；成功按 FIFO 传递 int。函数本身不等另一方改变状态。
调用方是否重试、yield 或采用其他等待策略属于外层策略。并发期间可以在本次观察到满/空后失败，再由调用方重试。

先用 seq_cst 建立正确基线，再为降低内存序逐一写理由。你需要证明两个方向：

```text
生产者写槽位 ── 发布/获取写位置 ── 消费者读该槽位
消费者读完槽位 ── 发布/获取读位置 ── 生产者再次写该槽位
```

第一条保证消费者看到已写的数据；第二条保证生产者不会在旧消费者还读槽位时覆盖它。
为每个普通槽位访问指出保护它的关系；仅让索引成为 atomic 不足以完成证明。
SPSC 的位置各有唯一写者，不要求对它们使用 CAS。解释这个限制被两个生产者破坏后会发生什么，暂不升级接口。

对象须在线程启动前构造、在线程全部 join 后销毁；没有并发 reset、close 或 resize。
角色线程可以在 join 之后更换。检查用确定的传输数量退出，这与 L06 的 close/drain 协议不同。
原子成员不无锁时，仍可讨论接口正确性，但不能把本机实现称为无锁。构造分配不属于运行中 try 操作。

## 5. 单项检查与 sanitizer（35min）

```bash
cmake --preset debug &&
cmake --build --preset debug --target lockfree_exercises_test --parallel 2 &&
timeout 20s ./build/debug/lockfree_exercises_test --help

./build/debug/lockfree_exercises_test budget_basic
./build/debug/lockfree_exercises_test spsc_single
timeout 20s ./build/debug/lockfree_exercises_test all
```

| 阶段 | checks |
|---|---|
| CAS 额度 | budget_basic、budget_zero、budget_overflow、budget_parallel、budget_parallel_chunks |
| 容量和普通 FIFO | spsc_invalid、spsc_empty、spsc_single、spsc_full、spsc_fifo |
| 回绕和内容 | spsc_wrap、spsc_values、spsc_random |
| 并发与线程交接 | spsc_transfer_small、spsc_transfer_wrap、spsc_handoff |

单线程随机检查以 std::deque 为 oracle；双线程检查每一个值和顺序，不只比较总和。
调用端的失败重试有期限；错误实现若在 try 操作内部自旋，仍需外部 timeout。
测试无法证明源码没有锁、没有动态分配或拥有进展保证；这些必须结合代码审阅和解释。

```bash
cmake --preset asan &&
cmake --build --preset asan --target lockfree_exercises_test --parallel 2 &&
timeout 20s ./build/asan/lockfree_exercises_test all

cmake --preset tsan &&
cmake --build --preset tsan --target lockfree_exercises_test --parallel 2 &&
timeout 20s ./build/tsan/lockfree_exercises_test all
```

在当前 Linux 遇到已观察到的 `ThreadSanitizer: unexpected memory mapping` 时，单独运行：

```bash
timeout 20s setarch x86_64 -R ./build/tsan/lockfree_exercises_test all
```

这是该次进程的兼容方式，不修改服务器全局 ASLR。TSan 无报告仍需配合两条同步关系的证明。

## 6. 测量与验收（45min）

完成核心并通过检查后：

```bash
cmake --preset release &&
cmake --build --preset release --target lockfree_benchmark --parallel 2 &&
timeout 20s ./build/release/lockfree_benchmark
```

固定容量 64，warmup 1024 个值，三轮各传输 20000 个值。计时包含线程创建、join、外层重试与 yield。
记录原子属性、次数、checksum 和三轮结果；这不是单条消息延迟，也没有“无锁必须比 mutex 快”的断言。
只启动一个生产者和一个消费者，不增加服务器压力来放大结果。

验收：16 项检查通过；解释 CAS 失败和加法溢出；画满/空/回绕；指出两条槽位同步关系和成功操作的提交点。
再回答以下追问：

1. 为什么使用 atomic_flag 写自旋锁，不等于写出了 lock-free 算法？
2. CAS 重试为何可能让某一个线程一直失败？SPSC 为什么可以省掉索引 CAS？
3. 如果传输元素改为可能分配内存的 std::string，整个操作的进展分析会有什么变化？
4. ABA 与悬空指针分别是什么问题？为何版本号不负责保住节点的生命周期？
5. 吞吐量、单条延迟、CPU 占用和公平性应如何分别测量？

深入 MPMC、hazard pointers/epoch 回收另计；完成本专项不要求手写它们。继续主线时回到尚未完成的 L11/L12。
