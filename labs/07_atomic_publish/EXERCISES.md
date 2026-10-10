# L07 动手练习与验收

先运行 [README](README.md) 的八组观察。这里只集中放实现任务，核心代码留给你。

| 阶段 | 修改文件 | 检查入口 |
|---|---|---|
| A：三线程接力，在原子操作中选择内存序 | [memory_order_exercises.hpp](memory_order_exercises.hpp) 中的 RelayPublication | memory_order_exercises_test，共 7 项 |
| B：直接发布普通 payload | [atomic_publication.hpp](atomic_publication.hpp) | atomic_publication_test，共 7 项 |

## 1. 构建与运行约定

```bash
cmake --preset debug &&
cmake --build --preset debug --target memory_order_exercises_test atomic_publication_test --parallel 2 &&
./build/debug/memory_order_exercises_test --help &&
./build/debug/atomic_publication_test --help
```

两个入口都支持 all、单个 case 名、--help；不带参数运行全部。
退出码 0 通过、1 检查失败、2 参数错误。初始 TODO 会明确失败，学员检查不加入默认 CTest。
每次修改头文件后重新编译。外部 timeout 超时通常为 124，不会给业务接口添加超时语义。

## 2. A：自己实现三线程接力（35–45 min）

只修改 RelayPublication 的核心函数，在 stage_ 的 load/store/exchange 中直接选择并解释内存序。
不复制 observer 的整段线程入口。
普通字段 source_、note_，原子 stage_；不用 mutex，不把两个普通字段改成 atomic。

| 接口 | 契约 |
|---|---|
| publish(source) | 唯一 source 调用一次；写 source，发布 stage=1 |
| try_forward(note) | 唯一 relay 可以重试；未到 1 或已经转发返回 nullopt，不改字段；成功后 stage=2，返回取得的 source |
| try_read() const | 只有阶段 2 才能返回完整 {source,note}，其他阶段立即返回 nullopt |

没有 reset，也没有多个 relay 争抢；成功转发后 source/note 不再修改。
先实现阶段判断和失败路径，再放置普通字段访问与 RMW，最后画完整同步链。
预检查本身不承担发布/接收。把 stage 改成 2 的同一次 exchange 必须承担两种方向的同步。

```bash
timeout 15s ./build/debug/memory_order_exercises_test initially_empty
timeout 15s ./build/debug/memory_order_exercises_test source_only
timeout 15s ./build/debug/memory_order_exercises_test forward_once
timeout 15s ./build/debug/memory_order_exercises_test early_relay
timeout 15s ./build/debug/memory_order_exercises_test concurrent_start
timeout 15s ./build/debug/memory_order_exercises_test multiple_readers
timeout 15s ./build/debug/memory_order_exercises_test independent_rounds
```

共 7 项，支持 all、单 case、--help；退出码 0/1/2 的约定与其他检查一致。
concurrent_start 创建工作线程后放行，普通字段写入发生在放行之后，不能借线程启动代替协议同步。
线程异常通过 future 收集，错误实现可能等不到阶段完成，外部 timeout 防止永久运行。

## 3. B：直接发布普通 payload（35 min）

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

| case | 验证内容 | 不能从这项单独推出什么 |
|---|---|---|
| initially_empty | 未发布时返回 nullopt，不去读未发布的数据 | 并发发布正确 |
| writer_first | 发布状态保留，全部字段一致 | release/acquire 必然正确；此项没有并发 |
| reader_first | 首次未发布，之后取得完整 payload | 调度或重试次数固定 |
| concurrent_start | 共同起点后的 writer 与 reader 能交付完整数据 | 某次成功覆盖了所有交错 |
| repeated_reads | 不可变 payload 能被重复读取 | 同一个对象能重新发布 |
| multiple_readers | 两个读者都取得同一份数据 | 一个 reader 能消费或删除它 |
| independent_rounds | 20 个新对象分别发布一次 | 反复切换同一个 ready 安全 |

reader_first 保证 reader 初次看到未发布，然后让 writer 发布。
其 latch 建立的顺序是 reader 初次检查到 writer 发布，不会替代 writer 到 reader 后续读取的同步。
concurrent_start 的公共起点也不能同步起点之后发生的 payload 写入与读取。

测试重试时用 yield，不用 sleep 规定先后；yield 不能建立 payload 的同步关系，也不保证公平。
测试的 packaged_task 收集线程异常，主线程报告失败前清理并 join；
错误实现仍可能永远等不到 ready，外部 timeout 防止运行无限挂起。

## 4. 必须交付的同步证明（20 min）

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

接力练习再分别画 source→relay、relay→reader，以及 source→reader 的关系。
指出哪次原子访问读取了哪次发布，并单独标注 source 与 note。
README 的 H 节中，弱化 exchange 的三个变体同样只做纸上分析。

## 5. 全部检查与动态验证

```bash
cmake --build --preset debug --target memory_order_exercises_test atomic_publication_test --parallel 2 &&
timeout 15s ./build/debug/memory_order_exercises_test all &&
timeout 15s ./build/debug/atomic_publication_test all
```

先通过单个接口的检查，再运行全部；两套练习的完成标准都是 7 项通过。
使用 TSan 检查已执行路径上的数据竞争：

```bash
cmake --preset tsan &&
cmake --build --preset tsan --target atomic_observe memory_order_exercises_test atomic_publication_test --parallel 2 &&
timeout 15s setarch x86_64 -R ./build/tsan/atomic_observe all &&
timeout 15s setarch x86_64 -R ./build/tsan/memory_order_exercises_test all &&
timeout 15s setarch x86_64 -R ./build/tsan/atomic_publication_test all
```

setarch 是此前 TSan 地址映射冲突的进程级兼容方式，不是同步机制。
reader 启动前已经写完的路径，可能被线程创建关系掩盖缺失的发布关系；
并发检查也不能枚举全部合法执行。错误实现可能等不到阶段完成，用外部 timeout 限制运行。
两个协议都没有 reset；独立轮次必须创建新对象。

## 6. 需要提交的解释

- writer 写 payload 到 reader 读 payload 的完整关系链，标出读到了哪次原子写。
- 为什么返回 nullopt 的 reader 不能同时读普通 payload。
- 为什么多个 reader 可以复读，而 writer 不能改写第二份。
- observer 的 relaxed 计数为何不能直接作为普通 payload 发布模板。
- split_update 为什么没有数据竞争却丢失一次更新；seq_cst 能否修复它。
- CAS 失败时 expected 与原子变量分别如何变化。
- ordering 中没有观察到 relaxed 的 00，为何不能证明它不可能出现。
- release/acquire 的 00 为什么也允许，G 的 true 与 F 的初始 0 有什么不同。
- acq_rel 的读取部分接收什么，写入部分发布什么；note 为什么要在 exchange 前写好。

另外说明 H 的 relay_read 为什么只由主线程在 join 后读取，不能声称它也被此前 stage=2 的写入发布。
说明只有一个 source、一个 relay、成功后字段不可变这些前提，以及 yield 的 CPU 成本与无公平保证。

## 7. 选做

比较 L05 的阻塞等待与当前 try_read 重试的成本。
需要 timeout 时使用单个 steady_clock 截止时间，不把计时、yield 或 sleep 当作同步。
在明确消费完成确认和对象生命周期协议前，不加 reset，也不扩展成无锁队列。
