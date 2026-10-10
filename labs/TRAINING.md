# L06–L12：怎样从观察进入独立实现

这些 lab 的核心实现都留给你。每个目录提供具体材料、接口契约、阶段任务、
脚手架和独立检查；README 是该 lab 的入口，不需要自己决定从哪个文件开始。
工程检查结果见 [验证记录](VALIDATION.md)，其中明确区分已完成观察与尚未实现的练习。

## 1. 按什么顺序做

先完成 L05 的三个 [同步练习](05_mutex_cv/EXERCISES.md)，随后按下面的顺序推进。
一次只做一个 lab 的基础阶段，不要求先实现剩下所有手写容器。

| 阶段 | 入口 | 你需要亲手完成的能力 |
|---|---|---|
| L06 | [有界阻塞队列](06_bounded_queue/README.md) | 空/满等待、关闭、排空、线程退出 |
| L07 | [atomic 发布](07_atomic_publish/README.md) | [三线程接力](07_atomic_publish/EXERCISES.md)，直接在原子操作中选择内存序；再完成直接发布并解释同步关系 |
| L08 | [OS 与内存](08_os_memory/README.md) | 按本机页大小定位和触碰页面，区分映射与首次访问 |
| L09 | [局部性](09_locality/README.md) | 正确访问同一组数据，校验结果后比较布局与访问顺序 |
| L10 | [共享与布局](10_false_sharing/README.md) | 控制共享关系与计数器布局，核对实际地址与总数 |
| L11 | [字节流与分帧](11_stream_framing/README.md) | 增量解析任意分片，处理部分发送和 EOF |
| L12 | [事件循环](12_event_loop/README.md) | 非阻塞读写、输出偏移、有限缓冲与背压；[epoll 的 LT/ET 专项练习](12_event_loop/EPOLL_EXERCISES.md) |

每个 lab 的扩展任务另计时间。基础阶段达标后继续主线，不用先完成所有性能调参、
硬件计数器、跨平台适配或生产系统功能。

## 2. 每次练习的动作

1. **先写契约**：输入、返回值、错误、对象由谁销毁；多线程组件写共享状态和等待条件。
2. **读一个材料片段**：只读当前阶段指定部分，随后回到代码。
3. **运行观察**：先预测输出含义，再解释实际输出。没有给定性能排序的实验不猜固定倍数。
4. **写一个接口**：删除该接口的 TODO 占位，自己实现；其他 TODO 可以暂时保留。
5. **运行一个 case**：先空状态和正常路径，再边界、异常、分片或关闭场景。
6. **说明正确性**：用状态变化或线程交错解释为什么成立，再记录动态检查结果。

采用 [实验记录模板](../docs/EXPERIMENT_TEMPLATE.md) 保存预测、证据和未排除的解释。
如果某个检查挂死，先定位等待谓词、通知和退出协议，不先增加 sleep。

## 3. 编译与运行的区别

所有命令在项目根目录执行。每个 README 给出了目标名和 case 名。

```sh
cmake --preset debug &&
cmake --build --preset debug --target bounded_queue_test --parallel 2 &&
timeout 10s ./build/debug/bounded_queue_test --help
```

用 --help 查看检查名称，随后将 --help 替换成单个名称；全部完成后使用 all。
新练习的检查入口统一约定：0 表示通过，1 表示检查失败，2 表示参数错误。
Linux timeout 超时通常返回 124；它检测程序挂死，不是组件 API 的业务超时。

TODO 占位会明确报错。**脚手架编译成功不表示组件已经实现。**
未完成练习不加入默认 CTest；CTest 中已实现的观察与学员检查分开。
可一次构建这一组脚手架，但日常仍建议只构建正在练的目标：

```sh
cmake --build --preset debug --target followup_exercises --parallel 2
```

macOS 如果没有 timeout 命令，可以直接运行单个 case；不要把 Linux 的辅助命令当作 C++ 接口。
需要排查挂死时使用调试器或具有外部超时的测试工具。POSIX 系统调用的支持范围见每个 lab。

## 4. 动态检查与性能观察

ASan/UBSan 用来排查内存与部分未定义行为；TSan 用来排查已执行路径上的数据竞态。
构建目标名不变，只改变 preset 和执行目录：

```sh
cmake --preset asan &&
cmake --build --preset asan --target bounded_queue_test --parallel 2 &&
timeout 10s ./build/asan/bounded_queue_test all
```

TSan 使用 tsan preset。若遇到此前的 unexpected memory mapping，且当前系统允许，
可以使用 L05 已说明的进程级兼容方式：

```sh
timeout 10s setarch x86_64 -R ./build/tsan/bounded_queue_test all
```

这些检查不代替同步证明。入口处的 latch 表示线程到达了调用前的事件，
不能单凭它证明线程已经阻塞在 CV 中；检查的具体限制见各 lab。

性能观察使用 Release，先保证 checksum 或总数正确。默认观察规模很小；
在共享服务器增加工作集、迭代次数或线程数之前先查看当前负载和内存余量。
不占满 CPU，不使用其他成员正在使用的 GPU，也不为这些练习下载大型数据。
记录编译器、优化模式、数据规模、线程数、地址/对齐、重复轮次及实际耗时。
不设置“随机访问必须慢多少”“隔开计数器必须快多少”这样的通过门槛。

L08–L10 另提供 memory_benchmark、locality_benchmark、false_sharing_benchmark，
它们调用你自己的实现。完成对应 header 后直接运行，无需先自行搭建测量框架；
未完成时明确报 TODO，不会替你调用参考答案。三个入口可一起构建：

```sh
cmake --preset release &&
cmake --build --preset release --target followup_benchmarks --parallel 2
```

基础检查先使用单个 case；计时入口的参数与工作量见各自 README。

## 5. 什么时候进入下一项

能独立实现基础接口、通过对应检查，并解释一个失败路径或边界，才算完成基础训练。
性能 lab 还需要一份包含测量限制的记录；网络 lab 还需要区分纯状态检查与真实 socket 检查。
日志通过、一次未报 sanitizer 错误和观察到某个耗时，分别只提供有限证据。

L12 的 parser/queue 综合接入是最后一轮训练。先通过各组件的基础检查，
再连接 L06/L11/L12；不要一开始把并发、分帧和 socket 错误混在同一个调试现场。
