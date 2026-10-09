# L06｜有界阻塞队列：等待、关闭与排空（7h）

这一步把 L05 的锁、等待谓词、通知和关闭组合成一个真正可用的组件。
你实现 [bounded_queue.hpp](bounded_queue.hpp)，检查由
[bounded_queue_test.cpp](bounded_queue_test.cpp) 提供。核心函数都保留 TODO。
这里不用先手写 deque，也不改成无锁队列；`std::deque<int>` 只负责元素存储。

完成后，你应该能说明：队列满时生产者如何等待，队列空时消费者如何等待，
关闭为什么必须考虑两组等待者，以及关闭后剩余数据由谁处理。

## 1. 从哪些材料开始（40 min）

| 材料 | 阅读位置 | 带着什么问题读 |
|---|---|---|
| [L05 练习](../05_mutex_cv/EXERCISES.md) | ResourceCounter 的消费与关闭 | 加入容量后，除了消费者，谁也需要等待？ |
| [OSTEP：Condition Variables](https://pages.cs.wisc.edu/~remzi/OSTEP/threads-cv.pdf) | Producer/Consumer (Bounded Buffer) 小节 | 为什么需要循环检查条件？两个 CV 分别表示什么？ |
| [C++ 条件变量规范](https://eel.is/c++draft/thread.condition.condvar) | wait(lock, pred)、notify_one、notify_all | 等待时是否持锁？返回时是否持锁？通知是否存储数据？ |

条件变量的带谓词等待会重复检查条件；条件不满足时释放锁并阻塞，醒来后重新取得锁。
通知让等待者有机会继续，业务状态由队列保存。因此通知早于等待也不能造成数据丢失。
这些是等待机制的依据；具体什么时候入队、拒绝或退出，由下面的契约决定。

不用通读所有并发章节，也不用在这一阶段掌握所有 atomic 内存序。

## 2. 先把业务契约写清楚（40 min）

只支持整数，不提供超时、取消、迭代器、复制和移动。

| 接口 | 开放时 | 关闭后 |
|---|---|---|
| BoundedQueue(capacity) | 容量必须大于 0；0 抛 std::invalid_argument | 不适用 |
| push(value) | 有空位时加入队尾并返回 true；满时等待 | 返回 false，不插入；即使满也不能继续等待 |
| pop() | 有元素时取队首；空时等待 | 有元素仍取队首；关闭且空才返回 std::nullopt |
| close() | 永久关闭，使等待者最终可退出 | 重复调用有效，不清空元素，不重新开放 |

例如容量 2，先入队 10、20，再关闭：后续两次 pop 分别得到 10、20，
第三次得到 nullopt。关闭后 push(30) 返回 false。
pop 的 nullopt 表示队列已经关闭且排空；整数 0 和负数都是正常数据。

owner 负责对象生命周期：**先 close，再结束并 join 所有使用线程，最后销毁队列**。
[jthread 的停止请求](https://eel.is/c++draft/thread.jthread.class)不能自动让普通 CV 等待退出；
析构队列也不能替代关闭和 join。

实现前填写这个表，暂时不要写函数体：

| 共享状态 | 初始值/约束 | 由谁保护 |
|---|---|---|
| capacity_ | 大于 0，创建后不变 | 构造完成后只读 |
| elements_ | 初始为空，数量不超过容量 | 你填写 |
| closed_ | 初始 false，之后只能变成 true | 你填写 |

自己写出两个等待谓词，并解释每个谓词中关闭状态的作用。
谓词成立只表示“可以做决定”，不保证一定能够成功入队或取到元素。

## 3. 准备工程并完成单线程阶段（60 min）

从项目根目录运行：

```sh
cmake --preset debug &&
cmake --build --preset debug --target bounded_queue_test --parallel 2 &&
./build/debug/bounded_queue_test --help
```

检查可选 all 或一个 case 名，默认 all。未实现的函数抛 TODO，检查会失败；
这是练习的起点，不要修改测试或把异常改成随意返回值。
练习没有加入默认 CTest，observer 的检查仍可以独立运行。

建议实现顺序：构造校验 → close → push → pop。
开始就为共享状态加锁；单线程阶段只简化验证顺序，不先写一个无锁版本。
等待、通知、锁范围都由你补齐，脚手架不提供核心实现。

```sh
timeout 15s ./build/debug/bounded_queue_test capacity_zero
timeout 15s ./build/debug/bounded_queue_test fifo
timeout 15s ./build/debug/bounded_queue_test close_empty
timeout 15s ./build/debug/bounded_queue_test close_drain
```

不要在同一个线程对满队列连续 push，期待后面的 pop 帮它解锁；
阻塞的 push 不返回，那个 pop 根本不会被执行。这是合法等待，不是队列 bug。

## 4. 单生产者、单消费者与两种等待（90 min）

先画两个时间线：空队列上的 pop 遇到一次 push；满队列上的 push 遇到一次 pop。
分别标明检查状态、释放锁等待、修改状态、通知、重新取得锁的时刻。

运行这些检查，每次只修复一个失败：

```sh
timeout 15s ./build/debug/bounded_queue_test reader_wake
timeout 15s ./build/debug/bounded_queue_test writer_wake
timeout 15s ./build/debug/bounded_queue_test transfer_one
timeout 15s ./build/debug/bounded_queue_test transfer_two
```

前两项在 close 之前等待操作完成，避免靠关闭掩盖普通 push/pop 漏掉通知。
后两项分别使用容量 1、2，传递 200 个整数，检查完整内容和先后顺序。

检查中的 latch 只表明线程到达调用前的事件，**不证明它已阻塞在 CV 内**。
合法实现应该同时支持“先等待后修改”和“先修改后等待”。一次测试通过不能证明覆盖了所有交错。
不要为了强行规定调度而在业务接口里加入 latch，也不要用 sleep 作为正确性条件。

## 5. 关闭能否让两组等待者退出（60 min）

分别运行：

```sh
timeout 15s ./build/debug/bounded_queue_test close_readers
timeout 15s ./build/debug/bounded_queue_test close_writers
```

每项都有两个等待者。关闭空队列时所有消费者应得到 nullopt；
关闭满队列时所有生产者应返回 false，先前入队的值仍然可以取出。

在纸上回答：

1. 正常加入一个元素、正常移走一个元素，各改变了哪组线程等待的状态？
2. close 改变的是一组还是两组线程的退出条件？
3. 一个等待者醒来之后，另一个线程可能先修改状态吗？
4. 如果 push 与 close 同时竞争锁，怎样判断这个 value 是否被接受？
5. 能不能只通知一次，然后依靠 jthread 的析构使其他线程退出？

不要保证等待者公平，也不要指定哪个线程先得到资源；本契约没有这类承诺。

## 6. 多生产者、多消费者与检查边界（60 min）

```sh
timeout 15s ./build/debug/bounded_queue_test multi_producer_order
timeout 15s ./build/debug/bounded_queue_test multi_consumer_ids
timeout 15s ./build/debug/bounded_queue_test all
```

第一项用两个生产者、一个消费者。每个生产者生成 100 个唯一 ID，
消费者检查每个生产者内部的顺序。两条生产者序列怎样交错都允许。
第二项用两个生产者、两个消费者，合并并排序消费结果，检查 200 个 ID 没有丢失或重复。

两个消费者各自 pop 返回后记录结果，其日志先后不能代表队列取出先后。
想验证全局 FIFO，需要在队列锁保护的取出位置记录线性化顺序；
当前脚手架没有这种日志接口，因此不声称多消费者测试证明了全局 FIFO。

测试线程通过 packaged_task 把结果或异常交回主线程；清理时先 close 再 join。
错误的等待或关闭实现仍可能挂死，所以命令设置外部 timeout，超时默认退出 124。
这不是 push/pop 的业务超时。不要捕获所有异常后吞掉检查失败。

## 7. 验收与后续方向（50 min）

具体任务和 12 项检查对照见 [EXERCISES.md](EXERCISES.md)。完成以下事项：

1. 全部检查通过，重复运行仍可退出。
2. 写出两个等待谓词、队列大小不变量、关闭不可逆的约束。
3. 指出成功 push、成功 pop、close 和失败返回各自作出最终决定的位置。
4. 画出关闭满队列和关闭空队列的线程退出过程。
5. 解释为什么通知不是数据，为什么测试入口的 latch 不能证明已经阻塞。

然后使用已实现代码运行检查：

```sh
cmake --preset asan &&
cmake --build --preset asan --target bounded_queue_test --parallel 2 &&
timeout 15s ./build/asan/bounded_queue_test all

cmake --preset tsan &&
cmake --build --preset tsan --target bounded_queue_test --parallel 2 &&
timeout 15s ./build/tsan/bounded_queue_test all
```

如果 TSan 遇到此前的 unexpected memory mapping，按 L05 的环境经验尝试
`timeout 15s setarch x86_64 -R ./build/tsan/bounded_queue_test all`。
测试和 sanitizer 只覆盖运行过的路径；它们不能替代状态和同步证明。

再进入 [L07](../07_atomic_publish/README.md)。选做方向：把整数扩展成可能抛异常的元素，
先定义取出失败时是否保留元素，再实现；比较行情快照和日志在队列满时应阻塞、丢弃还是拒绝。
不要求这个阶段实现无锁 MPMC、手写存储或业务性能基准。
