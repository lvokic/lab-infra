# 实验目录：12 个任务，约 48 小时

每个目录的 README 都含材料、步骤、验收和追问。已完成的观察程序运行于 CTest；学员练习提供接口、TODO 和独立检查，核心实现由你完成。L06–L12 的统一训练方式见 [从观察进入独立实现](TRAINING.md)。每项还需填写 [实验记录](../docs/EXPERIMENT_TEMPLATE.md)。

| ID | 主题 | 预算 | 当前代码 |
|---|---|---:|---|
| [L01](01_object_model/README.md) | 对象布局、virtual、析构和切片 | 4h | object_model 观察程序 |
| [L02](02_ownership/README.md) | RAII、copy/move、智能指针 | 3h | 自己实现 UniquePtr；test.cpp 检查所有权转移与生命周期 |
| [L03](03_vector/README.md) | vector 生命周期、扩容、MiniVector | 6h | vector_lifetime 观察程序 |
| [L04](04_containers/README.md) | iterator、hash/tree、容器选型 | 观察 2h；手写各项另计 | container_observe；[八套手写脚手架与检查](04_containers/SCAFFOLDS.md)，核心由你实现 |
| [L05](05_mutex_cv/README.md) | mutex/CV、谓词和交错 | 4h；补充练习另计 | cv_handshake 五组观察；[三个同步练习](05_mutex_cv/EXERCISES.md)，sync_exercises_test |
| [L06](06_bounded_queue/README.md) | 有界阻塞队列、关闭协议 | 7h | bounded_queue.hpp；bounded_queue_test 检查空/满、关闭、排空和多线程 |
| [L07](07_atomic_publish/README.md) | atomic 发布与同步关系 | 2h | atomic_observe；atomic_publication_test 检查一次性发布 |
| [L08](08_os_memory/README.md) | 进程/线程、VM、TLB、缺页 | 4h | memory_observe；memory_access_test；memory_benchmark 测量你的按页实现 |
| [L09](09_locality/README.md) | 局部性、布局、工作集 | 4h | locality_observe；locality_test；locality_benchmark 测量你的访问路径 |
| [L10](10_false_sharing/README.md) | 共享、对齐、一致性与测量 | 3h | false_sharing_observe；false_sharing_test；false_sharing_benchmark 测量你的布局与计数 |
| [L11](11_stream_framing/README.md) | 部分 I/O、framing、TCP loopback | 5h | stream_io；framing_test、stream_io_test 检查解析及真实 socket 传输 |
| [L12](12_event_loop/README.md) | nonblocking、poll、背压 | 4h；综合扩展另计 | poll_readiness 观察；event_loop_test 检查连接状态、输出缓冲和非阻塞 I/O |

上述脚手架目标已经接入构建；未完成练习不加入默认 CTest。可以先查看检查名称：

```sh
cmake --preset debug &&
cmake --build --preset debug --target bounded_queue_test --parallel 2 &&
timeout 10s ./build/debug/bounded_queue_test --help
```

你另外建立独立、已完成的观察实验时，可以在 CMakeLists.txt 添加：
```cmake
add_observation(bounded_queue labs/06_bounded_queue/solution.cpp concurrency)
```
然后将 bounded_queue 加入 .vscode/launch.json 和 tasks.json 的 labTarget 选项；或者始终使用 scratch 目标，避免改配置。

验收必须验证你的契约；不要根据容器的某个内部字段编写“正确性测试”。硬件计时结果用 Release，不设置“必须快多少”这样的 CTest 断言。
