# L07｜atomic 发布与 happens-before（2–2.5h）

L06 用 mutex 保护状态；这里练习一个更受限的协议：一个 writer 写一次普通 payload，
一个原子标志宣布它已经准备好，reader 观察标志后读取 payload。
你要说明这次读为什么安全，而不是凭某次输出正确作判断。

先运行 [observe.cpp](observe.cpp)，再自己实现
[atomic_publication.hpp](atomic_publication.hpp)。观察程序分六组演示原子操作与同步，
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

## 2. 观察：从原子操作到内存序（30 min）

```sh
cmake --preset debug &&
cmake --build --preset debug --target atomic_observe --parallel 2 &&
./build/debug/atomic_observe all
```

默认执行全部六组，也支持单项名称和 `--help`。先按下表逐项运行，读懂本组再进入下一组。

```sh
./build/debug/atomic_observe basics
./build/debug/atomic_observe split_update
./build/debug/atomic_observe rmw_counter
./build/debug/atomic_observe cas
./build/debug/atomic_observe mutex_counter
./build/debug/atomic_observe ordering
```

| 场景 | 运行前预测 | 运行后解释 |
|---|---|---|
| A `basics` | 初值 3，store 7，exchange 11，fetch_add 2：每次返回什么？ | load 读取；store 写入；exchange 和 fetch_add 返回更新前的值，最终为 13 |
| B `split_update` | 两个线程都先读到 0，然后分别 store 1，总数是多少？ | 最终为 1；单次访问是原子的，但 load、计算、store 的组合不是一次原子操作 |
| C `rmw_counter` | 两个线程各 fetch_add 1000 次 | 最终为 2000；读、修改、写作为一次不可分割的更新，称为 RMW |
| D `cas` | 当前 10，expected=7，尝试改成 20 | 第一次失败，expected 被改成 10；第二次成功，原子值改成 20 |
| E `mutex_counter` | 锁内对普通 int 做同样的递增 | 最终为 2000；mutex 保护整个操作，适合随后扩展为多个字段的不变量 |
| F `ordering` | 两个线程分别先写自己的原子变量，再读另一个 | 对比 relaxed 与 seq_cst 下四种读取组合；观察频率不能代替规范保证 |

### A–C：原子变量不等于任意操作组合都安全

先理解 `std::atomic<int>` 仍然保存一个整数，只是通过原子接口访问它。
`load`、`store` 各是一次操作；`fetch_add` 把“读取旧值并加上增量”合并为一次操作。
B 用 latch 确定性安排下面的交错，所有共享计数器访问都仍然是 atomic，没有 data race：

```text
线程 1：load 得到 0 ── 等待两个线程都读完 ── store 1
线程 2：load 得到 0 ── 等待两个线程都读完 ── store 1
最终值：1；两次递增的业务目标没有达到。
```

这是丢失更新。TSan 通常不会把这种原子访问的逻辑错误当作数据竞争报告。
即使把 B 的操作都改成 seq_cst，上面这种交错仍然成立；更强内存序不会把两个操作合成一个 RMW。

C 的 `is_lock_free` 表示当前对象的原子操作是否无锁，`is_always_lock_free` 表示该类型在本实现中是否始终无锁。
不要求所有平台都输出 1，也不能据此推断性能。C 和 E 没有做计时比较。

### D：CAS 中 expected 是输入，也是失败时的输出

CAS 是 compare-and-exchange。仅当原子变量等于 expected 时，把原子变量改为 desired。
失败时原子变量不被此次操作修改，但 expected 被更新为比较时读到的值。
观察代码使用 strong，让这个整数、单线程场景的结果确定。
weak 允许伪失败，通常放在重试循环中；暂时不把 CAS 扩展成无锁容器。
操作定义见 [atomic 类型操作规范](https://eel.is/c++draft/atomics.types.operations)。

### F：内存序约束的是不同操作之间的关系

每轮先把 x、y 归零，再让两个工作线程开始：

```text
线程 1：x.store(1, order) → r1 = y.load(order)
线程 2：y.store(1, order) → r2 = x.load(order)
```

输出 `reads_00/01/10/11` 是 128 轮中各 `(r1,r2)` 组合的次数。
relaxed 允许两个读取都得到 0，但本机不一定观察到；没有出现不代表它被禁止。
seq_cst 的这组操作需要符合共同的全序，加上线程内先后，不能同时读到 0：
若 r1 为 0，就要把线程 1 读取放在线程 2 写入之前；若 r2 也为 0，就形成顺序环。
本实验只要求 seq_cst 的 `reads_00 == 0`，不检查其他次数，也不要求 relaxed 出现 00。

`std::barrier` 是每轮的集合点，参与者为两个工作线程和主线程。
第一处等待让初始化完成后再开始；第二处让主线程在两个结果写完后统计。
它不在本轮两个线程的 store/load 之间额外规定顺序。

这里只用两个原子变量；release/acquire 与普通 payload 的同步由下一节练习完成。
它们的规则依据 [atomic 内存序](https://eel.is/c++draft/atomics.order)，需要画同步链解释。
不要把原子变量改成普通 int 来运行故意的数据竞争。

relaxed 可以适合单独的统计计数，但不因此保证另一个普通 payload 已可安全读取。
完成 A–F 后，自己回答：B 为什么没有 data race 却算错？CAS 失败时哪个变量改变？
为什么 C 的 relaxed 足够，而普通 payload 的发布还需要额外同步关系？

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
2. 解释六组 observer，分清原子访问、复合操作、内存序与普通字段发布。
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
