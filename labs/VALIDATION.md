# L06–L12 脚手架验证记录

日期：2026-10-09。环境：Linux x86_64，GNU C++ 13.3.0，C++20，Ninja。
本次验证的是教学工程和测试支撑，**学员核心算法仍为 TODO，没有完成答案**。

## 构建与检查

| 项目 | 本次结果 | 结果的范围 |
|---|---|---|
| 8 个学员检查入口 | Debug、ASan/UBSan、Release 构建成功 | 只证明脚手架与检查可编译 |
| 初始学员检查 | Debug/ASan 共 62 项均明确报 TODO，无挂死 | 预期失败，不是算法正确性通过 |
| 检查 CLI | --help、all、单 case、未知/多余参数符合约定 | Debug 下验证全部 8 个入口 |
| 新观察与 L11 原观察 | Debug、ASan/UBSan、Release 各 6/6 通过 | 5 个新观察，加原有 stream_io |
| L05 回归 | Debug 6/6 通过 | 原有 mutex/CV 观察 |
| TSan | 3/3 通过，无竞态报告 | atomic_observe、memory_observe、false_sharing_observe，使用 setarch x86_64 -R |
| 3 个学员计时入口 | Debug、ASan/UBSan、Release 可编译，并明确报 TODO | 未完成算法前不生成伪造性能结果 |
| locality_benchmark CLI | 帮助、合法边界、顺序、非法/重复/缺值参数已检查 | 数据规模限制生效，合法输入因 TODO 退出 1 |
| 工程一致性 | 本地链接、文档构建目标、VS Code 目标、clang-format、git diff --check 通过 | 新增/相关的文档与源码 |

TSan 使用此前 L05 的进程级兼容方式，不修改服务器的系统级 ASLR、代理或 DNS。
没有重新编译、验收所有其他容器，也没有更新你的学习完成分数。

## 学员检查清单

| Lab | target | case 数 |
|---|---|---:|
| L06 | bounded_queue_test | 12 |
| L07 | atomic_publication_test | 7 |
| L08 | memory_access_test | 4 |
| L09 | locality_test | 5 |
| L10 | false_sharing_test | 4 |
| L11 | framing_test | 12 |
| L11 | stream_io_test | 5 |
| L12 | event_loop_test | 13 |
| 合计 | | 62 |

计时入口为 memory_benchmark、locality_benchmark、false_sharing_benchmark。
观察程序使用默认小规模，不断言精确 fault 数、耗时或性能排序。

## 仍由学员完成并证明的部分

1. 全部 TODO 接口的正确实现，以及同步关系、生命周期、错误与关闭路径。
2. 完成算法后重新运行检查与 sanitizer；本记录不覆盖未来修改。
3. L11 的完整 parser/socket 组合和 L12 的 parser/整数队列/慢端综合练习。
4. 系统调用 EINTR 注入、真实异常连接和公平性等选做验证，具体边界见对应 README。
5. 目标硬件上的性能记录与替代解释；本次未做大工作集或压力实验。

从 [训练路线](TRAINING.md) 进入各 lab，按单个接口、单个 case 推进。
