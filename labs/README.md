# 实验目录：12 个任务，约 48 小时

每个目录的 README 都含材料、步骤、验收和追问。四个 observe.cpp 是安全的起始观察，正常 CTest 运行；更完整的实现由你写进 scratch/main.cpp 或各实验目录。每项还需填写 [实验记录](../docs/EXPERIMENT_TEMPLATE.md)。

| ID | 主题 | 预算 | 当前代码 |
|---|---|---:|---|
| [L01](01_object_model/README.md) | 对象布局、virtual、析构和切片 | 4h | object_model 观察程序 |
| [L02](02_ownership/README.md) | RAII、copy/move、智能指针 | 3h | 自己实现 UniquePtr；test.cpp 检查所有权转移与生命周期 |
| [L03](03_vector/README.md) | vector 生命周期、扩容、MiniVector | 6h | vector_lifetime 观察程序 |
| [L04](04_containers/README.md) | iterator、hash/tree、容器选型 | 2h | 自己实现 |
| [L05](05_mutex_cv/README.md) | mutex/CV、谓词和交错 | 4h | cv_handshake 观察程序 |
| [L06](06_bounded_queue/README.md) | 有界阻塞队列、关闭协议 | 7h | 自己实现 |
| [L07](07_atomic_publish/README.md) | atomic 发布与同步关系 | 2h | 自己实现 |
| [L08](08_os_memory/README.md) | 进程/线程、VM、TLB、缺页 | 4h | 自己实现 |
| [L09](09_locality/README.md) | 局部性、布局、工作集 | 4h | 自己实现 |
| [L10](10_false_sharing/README.md) | 共享、对齐、一致性与测量 | 3h | 自己实现 |
| [L11](11_stream_framing/README.md) | 部分 I/O、framing、TCP loopback | 5h | stream_io 观察程序 |
| [L12](12_event_loop/README.md) | nonblocking、poll、背压 | 4h | 自己实现 |

建立可运行实验后，可在 CMakeLists.txt 添加：
```cmake
add_observation(bounded_queue labs/06_bounded_queue/solution.cpp concurrency)
```
然后将 bounded_queue 加入 .vscode/launch.json 和 tasks.json 的 labTarget 选项；或者始终使用 scratch 目标，避免改配置。

验收必须验证你的契约；不要根据容器的某个内部字段编写“正确性测试”。硬件计时结果用 Release，不设置“必须快多少”这样的 CTest 断言。
