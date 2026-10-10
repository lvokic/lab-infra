# L07｜atomic、内存序与发布协议（3–4h）

L06 用 mutex 保护共享状态；本节先观察原子操作和五种内存序，再实现单次发布与三线程接力。
观察示例已完成；两套练习的核心仍由你写，不提供可执行的故意数据竞争版本。

本目录只保留两个教学文档：

| 文档 | 内容 |
|---|---|
| README.md（本文件） | 阅读材料、五种内存序、八组观察、barrier 与同步关系 |
| [EXERCISES.md](EXERCISES.md) | 接力与直接发布的实现步骤、内存序选择、检查、证明和验收 |

学习顺序：A–E 原子基础 → G release/acquire → H acq_rel → F 内存序对比与 barrier → 动手练习。
内存序描述访问之间的关系，不是等待时间，也不会自动把多个操作合成一个原子操作。

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

G 和直接发布练习使用 ready，初始 false，唯一一次写成 true；没有 reset，也没有第二次更新。
因此成功读到 true 可以明确对应这次发布。不要把这个结论直接推广到反复复用的对象。
内存序规则依据上述规范；其余限制是本实验刻意缩小的接口契约。

## 2. 五种内存序各负责什么

| 内存序 | 原子操作自身 | 关联访问的保证 | 本项目中的观察 |
|---|---|---|---|
| relaxed | 仍然原子 | 不通过该操作建立其他字段的同步 | C：独立统计计数 |
| release | 通常用于发布端 store | 把本线程此前访问接到后面的匹配 acquire | G：ready 写 true；H：source 写 stage=1 |
| acquire | 通常用于接收端 load | 读到匹配发布后，本线程后续访问获得同步保证 | G：看到 ready=true；H：看到 stage=2 |
| acq_rel | 用于同时读取并写入的 RMW | acquire 接收前一阶段，release 发布自己的先前访问 | H：exchange 把 stage 从 1 变成 2 |
| seq_cst | 仍然原子 | 具有相应 acquire/release 语义，并额外约束 seq_cst 操作的共同全序 | F：排除两个读取都为 0 |

这是本阶段使用的五种，不把它们简单排列成适合所有算法的“性能档位”。
C++20 还提供 consume，本练习不处理依赖链，只练上面五种。
定义见 [原子内存序](https://eel.is/c++draft/atomics.order) 和
[原子接口的合法参数](https://eel.is/c++draft/atomics.types.operations)。

store 不能使用 acquire/acq_rel；load 不能使用 release/acq_rel。
exchange/fetch_add/CAS 等读改写操作才同时有“读”和“写”两部分。
不要为了看不同输出而传入非法内存序；它不是一种有效的错误实验。

## 3. 八组观察：运行后解释每个保证

```sh
cmake --preset debug &&
cmake --build --preset debug --target atomic_observe --parallel 2 &&
./build/debug/atomic_observe all
```

默认执行全部八组，也支持单项名称和 `--help`。先按下表逐项运行，读懂本组再进入下一组。

```sh
./build/debug/atomic_observe basics
./build/debug/atomic_observe split_update
./build/debug/atomic_observe rmw_counter
./build/debug/atomic_observe cas
./build/debug/atomic_observe mutex_counter
./build/debug/atomic_observe ordering
./build/debug/atomic_observe release_acquire
./build/debug/atomic_observe acq_rel_relay
```

| 场景 | 运行前预测 | 运行后解释 |
|---|---|---|
| A `basics` | 初值 3，store 7，exchange 11，fetch_add 2：每次返回什么？ | load 读取；store 写入；exchange 和 fetch_add 返回更新前的值，最终为 13 |
| B `split_update` | 两个线程都先读到 0，然后分别 store 1，总数是多少？ | 最终为 1；单次访问是原子的，但 load、计算、store 的组合不是一次原子操作 |
| C `rmw_counter` | 两个线程各 fetch_add 1000 次 | 最终为 2000；读、修改、写作为一次不可分割的更新，称为 RMW |
| D `cas` | 当前 10，expected=7，尝试改成 20 | 第一次失败，expected 被改成 10；第二次成功，原子值改成 20 |
| E `mutex_counter` | 锁内对普通 int 做同样的递增 | 最终为 2000；mutex 保护整个操作，适合随后扩展为多个字段的不变量 |
| F `ordering` | 两个线程分别先写自己的原子变量，再读另一个 | 对比 relaxed、release/acquire、seq_cst；观察频率不能代替规范保证 |
| G `release_acquire` | value 的读写为 relaxed，ready 的发布/观察为 release/acquire | 获取对应的发布后，读取 value 得到 42；保证来自 ready 上的关系 |
| H `acq_rel_relay` | source 发布 1，relay 交换成 2，reader 获取 2 | relay 获取普通 source，同时把自己的普通 note 发布给 reader |

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

### G：为什么数据的 relaxed 读取也能得到保证

```bash
./build/debug/atomic_observe release_acquire
```

观察代码里的两个原子对象 value 与 ready：

```text
writer：value.store(42, relaxed)
                    ↓ 线程内先后
        ready.store(true, release)
                    │ reader 的 acquire 读到了这个 true
                    ↓
reader：ready.load(acquire)
                    ↓ 线程内先后
        value.load(relaxed) → 42
```

输出列出四次访问的内存序。value 的读取不是自己建立同步，是被 ready 上的同步关系保护。
唯一写入 42 先于这次读取，因此结果要求为 42。数据 value 也使用 atomic，是为了隔离本组的观察范围；
原有 OneShotPublication 才让你处理普通 payload。

先在纸上把 ready 的读写改成 relaxed：42 仍可能出现，但不再能用这条发布关系保证。
G 的 value 是 atomic，这种纸上变体不涉及普通字段的数据竞争；不过本项目不靠观察旧值次数证明结论。
不要把同样的弱化搬到普通 payload 上运行，它可能引入 UB。

主线程的 start latch 只放行本轮开始；读写发生在放行之后。
join 只保证主线程最后读取 observed，没有为工作线程中先前读取 value 建立额外同步。

### H：acq_rel 为什么需要“读改写”

```bash
./build/debug/atomic_observe acq_rel_relay
```

这组有三个角色，普通字段为 source、note，原子状态只走 0→1→2：

```text
source 线程：写 source=42 → stage.store(1, release)
                                        │ exchange 读取了 1：acquire 部分
relay 线程：写 note=7 → stage.exchange(2, acq_rel) → 读 source
                                        │ exchange 写入了 2：release 部分
reader 线程：                 stage.load(acquire) 读到 2 → 读 source、note
```

注意 relay 的两个不同方向：

- note 是 relay 自己准备的数据，在 exchange **之前**写好，由 release 部分发布。
- source 由别的线程写，在 exchange **之后**读取，由 acquire 部分接收。

relay 在 exchange 前用 relaxed 检查 stage=1，只用于决定现在能否操作。
它还没有资格凭这个预检查去读取普通 source；那次 exchange 读到 1 后才建立所需的 acquire 关系。
这里只允许一个 source、一个 relay，stage 没有其他写者，所以预检查到 1 后不会被其他 relay 抢走。

输出 old_stage=1、relay_read=42、received_source=42、received_note=7。
不要在 exchange 之后才写 note：reader 可能已经看到 2 并开始读 note。
同样不要把 observer 中 join 后打印的 relay_read 当作也被 stage=2 发布给了 reader；
它在 exchange 之后才写，只由主线程在 join 后读取。

仅在纸上分析：

1. exchange 改为 release，relay 自己读取 source 缺少什么？
2. exchange 改为 acquire，reader 读取 note 缺少什么？
3. exchange 改为 relaxed，两处访问分别如何证明或无法证明？

source 的 release 还可能经 RMW 的 release sequence 被下游 acquire 观察到。
因此不能笼统说“弱化 relay 后所有数据都一定读错”；要逐个字段指出缺失的关系。
本练习明确要求 relay 本身获取 source，并发布自己的普通 note，使用 acq_rel 表达这两个角色。
弱化后的普通字段变体只做关系分析，不运行故意数据竞争。

### F：release/acquire 与 seq_cst 不是同一回事

```bash
./build/debug/atomic_observe ordering
```

F 比较三种配置：relaxed/relaxed、release-store/acquire-load、seq_cst/seq_cst。
两线程分别写 x/y 为 1，再读对方：

```text
线程 1：x.store(1, store_order) → r1 = y.load(load_order)
线程 2：y.store(1, store_order) → r2 = x.load(load_order)
```

reads_00/01/10/11 是 128 轮中各 (r1,r2) 组合的次数。

| 配置 | 两个读取都为 0 是否允许 | 为什么 |
|---|---|---|
| relaxed | 允许 | 没有借这些操作建立跨线程发布关系 |
| release/acquire | 允许 | 两次 acquire 都读到初始 0，没有读到对方 release 发布的 1 |
| seq_cst | 本场景不允许 | 四个 seq_cst 操作的共同顺序和线程内顺序无法同时满足 00 |

G 则是确认读取了发布的 true 后才读取数据；F 没有这个确认条件。
所以不能照搬 G 的保证到 F。本机可能三行 reads_00 都是 0；那只是本次采样，没有改变规范允许的结果。
不加 sleep 去制造“内存序一定输出不同”的假象，也不以次数或速度给内存序打分。

若 seq_cst 下 r1 为 0，就要把线程 1 的读取放在线程 2 的写入之前；
若 r2 也为 0，加上两线程各自先写后读，就形成顺序环。
本实验只检查 seq_cst 的 reads_00 为 0，不限定其他次数，也不要求较弱配置一定出现 00。

### F 中的 barrier：阶段边界的集合点

`std::barrier` 是 C++20 的线程同步工具。本实验把参与者数量固定为三个；
每一轮里，每个参与者到达 barrier 并等待，直到所有参与者都到达，随后大家才能继续。
这一轮的 barrier 就像一个集合点。

它常用于分阶段并行工作：先让所有线程完成阶段 A，再一起进入阶段 B。
barrier 也提供阶段之间的同步，因此 barrier 前完成的写入能被越过该 barrier 后的代码观察到。
它不会替你保护任意共享数据；同一阶段里，多个线程仍可能并行访问共享状态。

`observe.cpp` 中的 `std::barrier phase(3)` 有三个参与者：主线程和两个工作线程。每轮有两个集合点：

1. 主线程先把 `x`、`y` 清零；三方通过第一个 barrier 后，两个工作线程开始本轮读写。
2. 工作线程写好各自的读取结果后到达第二个 barrier；主线程等两方都到达后再统计结果。

barrier 让一组线程在阶段边界会合，并同步 barrier 前后的阶段。
原子内存序则约束原子操作之间的可见性和顺序关系。
barrier 不会把 `relaxed` 自动变成 `release/acquire`，
也不会替 `ordering` 中的 store/load 增加线程内顺序以外的全局顺序。

本实验中的 barrier 用来重置每轮状态并安全收集结果；它刻意没有消除要观察的读写交错。

## 4. 从观察进入练习

理解 G 的直接发布与 H 的接力关系后，按 [EXERCISES.md](EXERCISES.md) 实现
RelayPublication 和 OneShotPublication，并解释原子操作中的内存序选择。
未观察到某个结果，不代表规范禁止它；
没有 TSan 报告，也不能代替每个普通字段的 happens-before 证明。

完成本节基础后进入 [L08](../08_os_memory/README.md)。
SPSC ring buffer、atomic wait/notify 和可重复发布另作扩展，先写生命周期与退出契约。
