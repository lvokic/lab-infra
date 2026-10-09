# L07｜atomic 发布与 happens-before（2h）

L06 用 mutex 保护状态；这里练习一个更受限的协议：一个 writer 写一次普通 payload，
一个原子标志宣布它已经准备好，reader 观察标志后读取 payload。
你要说明这次读为什么安全，而不是凭某次输出正确作判断。

先运行 [observe.cpp](observe.cpp)，再自己实现
[atomic_publication.hpp](atomic_publication.hpp)。观察程序只演示原子计数，
不会提供一次性发布练习的核心实现。没有可执行的故意 data race 版本。

## 1. 材料与需要掌握的三个词（20 min）

| 材料 | 阅读位置 | 目的 |
|---|---|---|
| [C++：atomic 内存序](https://eel.is/c++draft/atomics.order) | relaxed、release、acquire，以及 release 与 acquire 的同步关系 | 区分原子操作本身与关联普通字段 |
| [C++：数据竞争与执行顺序](https://eel.is/c++draft/intro.races) | sequenced-before、happens-before、data race | 为一次 payload 读写建立关系 |
| [L05](../05_mutex_cv/README.md) | join 后读取结果、锁与等待 | 对比已有同步方法 |

先记住本实验使用的范围：

- **sequenced-before**：同一线程中，本次协议明确规定的先后操作。
- **synchronizes-with**：本协议中，读取对应 release 写入值的 acquire 操作与其建立同步。
- **happens-before**：把线程内先后与跨线程同步连接起来，说明普通字段的读写有顺序。

这里有一个 ready，初始 false，唯一一次写成 true；没有 reset，也没有第二次更新。
因此成功读到 true 可以明确对应这次发布。不要把这个结论直接推广到反复复用的对象。
内存序规则依据上述规范；其余限制是本实验刻意缩小的接口契约。

## 2. 观察：relaxed 仍然是 atomic（15 min）

```sh
cmake --preset debug &&
cmake --build --preset debug --target atomic_observe --parallel 2 &&
./build/debug/atomic_observe
```

两个线程各对同一个 atomic<int> 做 1000 次 relaxed fetch_add，join 后检查总数为 2000。

```text
[atomic counter] value=2000 expected=2000 is_lock_free=... is_always_lock_free=...
[scope] counter only; no ordinary payload is published by this observer
```

| 字段 | 应该怎样理解 |
|---|---|
| value / expected | 本实验只有一个原子计数器，更新不能丢失 |
| is_lock_free | 当前对象的原子操作是否由实现保证无锁 |
| is_always_lock_free | 当前类型在该实现中是否始终无锁 |

这些属性的定义见 [atomic 类型操作规范](https://eel.is/c++draft/atomics.types.operations)。
不要求它们在所有平台都为 1，也不从它们推断延迟或整个程序是无锁算法。
relaxed 可以适合单独的统计计数，但不因此保证另一个普通 payload 已可安全读取。
这个 observer 的工作线程已经 join，且没有发布任何关联的普通共享数据。

把 per_worker 从 1000 改成 100、5000，预测最终总数再运行；
不要把 counter 改成普通 int 来尝试运行 data race。

## 3. 固定一次性发布契约（15 min）

PublicationPayload 有普通整数 sequence 和四个普通 samples，检查要求整组值完整一致。

| 接口 | 契约 |
|---|---|
| publish(value) | 唯一 writer 在对象一生只调用一次；完成后 payload 永不再修改 |
| try_read() const | 未发布时立即返回 nullopt；已发布时返回完整 payload 副本；接口不阻塞 |

允许多个 reader 和重复读取。没有抢占发布、失败发布、关闭、超时或第二次发布接口。
这些不是让你遗漏检查，而是调用方的前提；测试也遵守它们。
析构前 owner 必须结束并 join 所有使用线程。

只用一个 atomic<bool> 和普通 payload；不要加 mutex，也不要把每个普通字段都改成 atomic。
保留 TODO 的内存序选择，由你实现和解释。

## 4. 自己实现并分阶段运行（35 min）

```sh
cmake --build --preset debug --target atomic_publication_test --parallel 2 &&
./build/debug/atomic_publication_test --help
```

只修改 [atomic_publication.hpp](atomic_publication.hpp)，按以下顺序做：

1. 先完成未发布时 try_read 的行为。
2. 决定 writer 中“写 payload”和“改变 ready”的顺序与内存序。
3. 决定 reader 中“读 ready”和“读 payload”的顺序与内存序。
4. 在纸上完成下一节的证明，再运行并发检查。

```sh
timeout 15s ./build/debug/atomic_publication_test initially_empty
timeout 15s ./build/debug/atomic_publication_test writer_first
timeout 15s ./build/debug/atomic_publication_test reader_first
timeout 15s ./build/debug/atomic_publication_test concurrent_start
timeout 15s ./build/debug/atomic_publication_test repeated_reads
timeout 15s ./build/debug/atomic_publication_test multiple_readers
timeout 15s ./build/debug/atomic_publication_test independent_rounds
```

共 7 项检查，默认 all，也支持一个 case 名。未实现时明确失败，练习不加入默认 CTest。
reader_first 保证 reader 初次看到未发布，然后让 writer 发布。
其 latch 建立的顺序是 reader 初次检查到 writer 发布，不会替代 writer 到 reader 后续读取的同步。
concurrent_start 的公共起点也不能同步起点之后发生的 payload 写入与读取。

测试重试时用 yield，不用 sleep 规定先后；yield 不能建立 payload 的同步关系，也不保证公平。
测试的 packaged_task 收集线程异常，主线程报告失败前清理并 join；
错误实现仍可能永远等不到 ready，外部 timeout 防止运行无限挂起。

## 5. 必须交付的同步证明（20 min）

自己填下面的关系，不要只写“atomic 所以线程安全”：

```text
writer: 写普通 payload  →  发布 ready
                                  │
                           reader 读到了哪次写入？
                                  │
reader: 读取 ready      →  读取普通 payload
```

在两条横线标 sequenced-before；在中间填同步关系及成立条件；
最后连接成“写 payload happens-before 读 payload”。
解释未读到 true 时为什么不能提前读取 payload，为什么发布之后必须保持 payload 不变。

然后仅在纸上分析三个变体，不编译运行它们：

1. ready 的写和读都改成 relaxed，缺少哪条边？
2. 先发布 ready，再填写普通 payload，哪次普通访问失去顺序保证？
3. reader 还在读取第一份 payload，writer 已开始写第二份，为什么一次发布关系不足以保护这两次访问？

第三种情况需要新的复用/消费完成协议。换成 seq_cst 标志也不能让普通字段的并发改写自动安全。
先不实现这样的复用，更不扩展成无锁队列。

## 6. 验收与下一步（15 min）

详细检查对照见 [EXERCISES.md](EXERCISES.md)。验收要求：

1. 7 项检查通过，且能自己画出完整 happens-before 链。
2. 分清 observer 的原子计数与练习的普通字段发布。
3. 说明只有一个 writer、一次发布、发布后不可变三个前提。
4. 解释为什么 repeated_reads 是安全复读，而 independent_rounds 必须创建新对象。
5. 说明重试与 yield 的 CPU 成本；本实验不声称有等待时限或公平保证。

```sh
cmake --build --preset debug --target atomic_publication_test --parallel 2 &&
timeout 15s ./build/debug/atomic_publication_test all

cmake --preset tsan &&
cmake --build --preset tsan --target atomic_publication_test --parallel 2 &&
timeout 15s ./build/tsan/atomic_publication_test all
```

此前的 TSan mapping 问题可尝试
`timeout 15s setarch x86_64 -R ./build/tsan/atomic_publication_test all`。
没有竞态报告不能代替同步证明；特别是 reader 启动前已经写完的路径，容易由线程创建关系掩盖缺失的发布关系。

接下来进入 [L08](../08_os_memory/README.md) 的系统观察。
SPSC ring buffer、atomic wait/notify 和可重复发布都是选做，先写生命周期与退出契约再开始。
