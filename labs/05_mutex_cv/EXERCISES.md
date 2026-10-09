# L05 动手练习：从锁到可关闭的等待

读完 observer 后，按下面三个阶段实现 [sync_exercises.hpp](sync_exercises.hpp)。
接口、共享字段和检查已经提供，锁、等待谓词、通知及关闭逻辑都由你实现。
不用修改 [sync_exercises_test.cpp](sync_exercises_test.cpp) 来让检查通过。

每个 TODO 暂时抛出 logic_error，所以脚手架可以编译，但运行检查会失败。
先完成一个类，再运行对应分组；不要一开始就写完整阻塞队列。

## 1. 编译和分组检查

在项目根目录执行：

```sh
cmake --preset debug &&
cmake --build --preset debug --target sync_exercises_test --parallel 2 &&
timeout 10s ./build/debug/sync_exercises_test stats
```

实现下一个类时，把最后的 stats 改成 result 或 resources：

```sh
timeout 10s ./build/debug/sync_exercises_test result
timeout 10s ./build/debug/sync_exercises_test resources
```

也可以只运行一项检查：

```sh
timeout 10s ./build/debug/sync_exercises_test result_waiters
./build/debug/sync_exercises_test --help
```

修改头文件后要重新构建。全部完成后运行：

```sh
cmake --build --preset debug --target sync_exercises_test --parallel 2 &&
timeout 10s ./build/debug/sync_exercises_test all
```

共 12 项检查。退出码 0 表示通过，1 表示检查失败，2 表示参数错误。
Linux 的 timeout 默认在超时后返回 124，用来发现挂死；它不是 acquire/wait 接口的业务超时。
未完成的练习没有注册进默认 CTest，避免影响现有 observer 的检查。

## 2. SharedStats：保护一组相关字段

目标：多个线程记录任务，每条记录同时影响 tasks 和 total_amount。
一个查询返回两个字段的一致快照。

| 接口 | 契约 |
|---|---|
| record(amount) | 增加一条任务记录，并把金额计入总金额 |
| snapshot() const | 返回同一临界区内读取的两个字段，不能观察到半次更新 |

初始两个字段都是 0。本练习金额非负，累计值不会溢出；不需要扩展输入校验。
mutex_ 已声明为 mutable，因为 const 查询同样需要同步。

先自己回答，再开始写：

1. 哪几个字段由 mutex_ 保护？record 的临界区应该覆盖哪些操作？
2. 如果修改有锁，而 snapshot 无锁，会有什么问题？
3. 如果给两个字段分别加锁，两次读取之间是否可能插入一条新记录？

实现顺序：snapshot → record → 并发检查。

| 检查 | 验证内容 |
|---|---|
| stats_initial | 空快照 |
| stats_records | 包括金额 0 的单线程记录 |
| stats_concurrent | 两个写线程各记录 2000 次，主线程同时查询；没有丢失更新，快照保持金额与任务数的关系 |

完成后解释为什么两个字段各自读起来合理，拼起来却可能不是一致快照。

## 3. OneShotResult：等待一次发布

目标：一个整数结果只发布一次，可以有多个发布者竞争、多个读者等待。

| 接口 | 契约 |
|---|---|
| publish(value) | 第一次成功，保存 value 并返回 true；后续返回 false，不覆盖结果 |
| wait() | 未发布就等待；已发布就返回整数，不消耗结果 |

初始 ready_ 为 false。对象没有 reset；重复读取获得同一个结果。
至少有一次成功发布，等待者才能正常退出；析构前必须结束并 join 所有使用者。

先在纸上写出：

1. wait 的等待谓词是什么？检查它时必须持有哪把锁？
2. publish 的哪些状态修改必须作为一次操作完成？
3. 两个读者都在等待时，只让其中一个得到唤醒机会是否足够？
4. publish 先发生时，wait 应不应该依赖过去的通知？

实现顺序：publish → wait → 多等待者 → 多发布者。

| 检查 | 验证内容 |
|---|---|
| result_early | 先发布后等待，以及重复读取 |
| result_duplicate | 第二次发布被拒绝，保留原结果 |
| result_waiters | 两个读者都能取得同一个结果 |
| result_publishers | 两个发布者恰好一个成功，保存获胜者的值 |

这一步练等待协议，不需要实现 future/promise；检查代码使用 future 只是为了收集线程结果和异常。

## 4. ResourceCounter：消费与关闭

目标：只维护资源数量，没有元素存储，也没有容量限制。
每次 acquire 成功消耗一份资源；多个消费者不能重复取得同一份。

| 接口 | 契约 |
|---|---|
| add() | 开放时增加一份，返回 true；关闭后拒绝，返回 false |
| acquire() | 有资源就消耗一份，返回 true；空且开放时等待；关闭且空时返回 false |
| close() | 永久关闭；可重复调用；使所有等待者最终退出 |

关闭不丢弃已有资源：例如先添加两份再关闭，接下来两次 acquire 成功，第三次失败。
初始 available_ 为 0、closed_ 为 false；本练习添加总量不会溢出。

先自己写出三种状态的行为：开放且空、开放且有资源、关闭。
关闭还要区分有资源与无资源。然后回答：

1. acquire 的等待谓词是什么？等待谓词成立后为什么仍需区分消费与退出？
2. 检查数量和减去一份之间，能不能释放锁？
3. add 与 close 分别需要让哪些等待者有机会重新检查条件？
4. add 和 close 并发时，怎样决定这份资源是否被接受？

实现顺序：add → close → acquire → 多消费者。

| 检查 | 验证内容 |
|---|---|
| resources_drain | 关闭保留两份资源、拒绝添加、重复关闭 |
| resources_closed_empty | 关闭且空时重复获取都立即失败 |
| resources_wake | 添加一份后，获取线程在关闭前完成 |
| resources_close_waiters | 关闭空对象后，两个读者都退出 |
| resources_concurrent | 两个消费者合计取得 100 份，无丢失、无重复，关闭后排空 |

不要要求两个消费者均分资源，或指定哪个先取得锁。

## 5. 检查的边界与完成标准

检查中的 latch 只协调工作线程进入调用前的事件，**不证明线程已在 CV 内阻塞**。
started 归零后，线程仍可能尚未执行 wait/acquire。检查覆盖多种合法启动顺序；
某次通过不能证明通知和等待的所有交错都正确。需要确定性观察首次 false 检查时，
参考 observer 的 B/C/D；不用为了这些检查把 latch 加到练习的业务接口里。

没有使用 sleep 保证线程顺序，也不对执行耗时或获胜线程作断言。
工作线程不打印、不直接抛出到线程入口；std::async 把异常交给主线程的 get() 报告。
future 析构可能等待工作线程完成，错误的等待协议仍可能挂死，所以运行命令有外部超时。

完成以下事项再进入 L06：

1. 三组共 12 项检查通过。
2. 每个类写一行共享状态不变量，标明每个字段由哪把锁保护。
3. 不看实现，写出两个等待谓词，并解释通知早于等待和关闭时的行为。
4. 画出两个消费者争一份资源的交错，以及两个发布者竞争时只成功一个的交错。

最后可检查已实现代码的内存和竞态：

```sh
cmake --preset asan &&
cmake --build --preset asan --target sync_exercises_test --parallel 2 &&
timeout 10s ./build/asan/sync_exercises_test all

cmake --preset tsan &&
cmake --build --preset tsan --target sync_exercises_test --parallel 2 &&
timeout 10s ./build/tsan/sync_exercises_test all
```

若 TSan 遇到你此前的 unexpected memory mapping，可按本节 README 的环境说明尝试：

```sh
timeout 10s setarch x86_64 -R ./build/tsan/sync_exercises_test all
```

动态检查只覆盖已执行路径。下一步 [L06](../06_bounded_queue/README.md) 再加入元素存储、
容量上限和生产者等待，形成完整有界阻塞队列。
