# L06 动手任务与检查对照

只修改 [bounded_queue.hpp](bounded_queue.hpp) 的 TODO。
详细材料、阶段说明和命令在 [README](README.md)。

## 实现前交付一页设计

写出共享字段与锁的对应关系；列出开放/关闭、空/非空/满的行为。
容量 1 时“非空”同时也是“满”，不要把这些状态误认为互斥枚举。
补两个等待谓词和三种修改后的通知计划，暂时不用写 C++。

## 逐项检查

| 次序 | case | 检查内容 | 失败先看哪里 |
|---|---|---|---|
| 1 | capacity_zero | 0 容量必须抛 invalid_argument | 构造校验 |
| 2 | fifo | 三个正负整数按入队顺序取出 | 队首/队尾、整数与 nullopt |
| 3 | close_empty | 重复关闭，拒绝 push，空 pop 不再等待 | 关闭是否永久、是否幂等 |
| 4 | close_drain | 关闭满队列仍保留原有元素 | 是否误把 close 写成 clear |
| 5 | reader_wake | 一次 push 使空队列读取完成 | 消费者条件及入队后的通知 |
| 6 | writer_wake | 一次 pop 为容量 1 的第二次 push 腾出空位 | 生产者条件及取出后的通知 |
| 7 | close_readers | 关闭后两个读取者都退出 | 消费者的关闭分支与通知范围 |
| 8 | close_writers | 关闭后两个写入者都拒绝，不覆盖原数据 | 生产者的关闭分支与通知范围 |
| 9 | transfer_one | 容量 1 传递 200 个值 | 满/空切换与顺序 |
| 10 | transfer_two | 容量 2 传递 200 个值 | 非空与非满可以同时成立 |
| 11 | multi_producer_order | 两个生产者的唯一 ID 与各自顺序 | 共享存储是否完整受锁保护 |
| 12 | multi_consumer_ids | 两个生产者、两个消费者无丢失无重复 | 检查条件与取出是否同属临界区 |

运行单项示例：

```sh
cmake --build --preset debug --target bounded_queue_test --parallel 2 &&
timeout 15s ./build/debug/bounded_queue_test close_writers
```

不要一上来运行 all 并同时改多个函数。某项挂死时先看最后的 `[RUN]`，
检查等待谓词、状态修改和通知，而不是加 sleep 或把等待改成空循环。

## 完成后的代码外作业

1. 选一个成功 push 与一个 close 的竞争，指出成功/失败决定发生在哪里。
2. 画出两个消费者争一个元素，解释唤醒后为什么仍要检查条件。
3. 解释单消费者顺序检查与多消费者 ID 检查各自能证明什么。
4. 写出 owner 的 close/join/销毁顺序，解释只请求线程停止为什么不够。

选做：只改容量为 1、2、8，保持唯一 ID 检查。不要要求线程均分工作。
想加 try_push 或 timed pop 时先写新契约；它们不是当前必做接口。
