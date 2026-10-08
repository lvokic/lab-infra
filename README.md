# Quant Developer Interview Lab

面向量化公司 Software Developer / Quant Developer 技术面试的 C++20 学习实验。根据你已刷过 Top 150 的进度，训练重心改为对象模型、STL、并发、OS/内存体系结构、网络与 I/O。

## 从这里开始

| 文件 | 用途 |
|---|---|
| [四周计划](docs/PLAN.md) | 100 小时预算、20 个学习单元、优先级、提前面试压缩方案 |
| [视频与材料](docs/RESOURCES.md) | 指定视频/章节、看什么、对应实验、时间预算 |
| [12 个实验](labs/README.md) | API 契约、练习步骤、边界验证、面试追问 |
| [本机 STL 源码地图](docs/STL_SOURCE_MAP.md) | Xcode SDK 的 vector、智能指针、哈希表、树阅读入口 |
| [面试自测](docs/INTERVIEW.md) | 46 个机制问题及追问、三条综合模拟链 |
| [进度与错题](docs/PROGRESS.md) | 实际用时、0–3 分自测、间隔复习 |
| [实验记录模板](docs/EXPERIMENT_TEMPLATE.md) | 预测、证据、反例、口述与重测 |

优先完成生命周期/所有权、vector、mutex/CV 和有界队列；然后练虚拟内存/cache 和 TCP framing/背压。算法每周保留 2 小时限时复测。建议顺序和调整规则以四周计划为准；资料是定点输入，无需通看整门课程。

## 在这台 Mac 上运行

本机：ARM64、Apple Clang 17、CMake 4.1.2、Ninja 和 LLDB。使用 C++20，没有第三方测试框架依赖。VS Code 已有 C/C++、CMake Tools、CodeLLDB 扩展。

在 VS Code 中用 File → Open Folder 打开 ~/quant-dev-interview-lab；终端 code 命令目前未配置，可通过命令面板 Shell Command: Install 'code' command in PATH 添加。

```sh
cd ~/quant-dev-interview-lab
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

如果你此前在项目根目录执行过 cmake . 并生成 Makefile，可以继续 make；这类构建的程序会输出到 build/in-source/，例如 ./build/in-source/object_model。根目录旧可执行文件可能是此前产物，运行新版时使用上述输出路径。日常建议使用上面的 debug preset，F5 也使用 build/debug/。

已提供可运行的**观察起点**，以及 unique_ptr 的练习脚手架；它们不是十二个练习的完成答案：

| 目标 | 源文件 | 观察内容 |
|---|---|---|
| object_model | [L01/observe.cpp](labs/01_object_model/observe.cpp) | 构造期间派发、切片、多态析构、大小与对齐 |
| unique_ownership_test | [L02/test.cpp](labs/02_ownership/test.cpp) | 检查手写 UniquePtr 的创建、移动、release、reset 和对象生命周期 |
| vector_lifetime | [L03/observe.cpp](labs/03_vector/observe.cpp) | reserve/resize/clear、存活对象、复制移动 |
| container_observe | [L04/observe.cpp](labs/04_containers/observe.cpp) | map/unordered_map 接口、哈希冲突、rehash 与元素稳定性 |
| cv_handshake | [L05/observe.cpp](labs/05_mutex_cv/observe.cpp) | 锁保护的谓词和 payload、额外通知、join |
| stream_io | [L11/observe.cpp](labs/11_stream_framing/observe.cpp) | 本地字节流、分段读写、长度字段、EOF |
| scratch | [scratch/main.cpp](scratch/main.cpp) | 你自己的新实验；不计入 CTest |

每个观察程序内有 require 检查，Release 下仍有效。它们验证特定语义路径，不代替学习任务中的边界测试或并发证明。2026-09-30 本机验证：Debug 和 Release 构建成功，各自 4/4 项 CTest 通过；VS Code JSON、目标选项和本地文档链接检查通过。本次未操作 VS Code GUI 验证 F5。

## F5 调试和日常练习

1. 先读对应实验 README，预测输出或线程交错。
2. 在 observe.cpp 或 scratch/main.cpp 设置断点。
3. F5 选择 Debug a systems experiment (LLDB)，再选择目标；启动前自动配置和构建 Debug。
4. 看变量、对象地址、调用栈；一次只改变一个条件。
5. 用 Terminal → Run Task → CMake: Test Debug 跑四个观察检查；Run a systems experiment 可直接选目标运行。
6. 将实现和边界验证加入自己的实验，填记录模板；48 小时后闭卷重答。

新增独立目标时，在 CMakeLists.txt 使用 add_observation，并更新 launch/tasks 的目标选项；也可以先一直使用 scratch。具体步骤见 [实验索引](labs/README.md)。

## C++ 代码格式

统一使用 clang-format 18.1.8，规则在 [.clang-format](.clang-format)：两空格缩进、100 列、左侧 `T*` / `T&` 写法，非空函数、条件和循环体展开到多行。保留 include 顺序和教学注释的原有分行。规则说明见 [clang-format 官方文档](https://clang.llvm.org/docs/ClangFormatStyleOptions.html)。

首次安装无需 root；当前 Linux 环境已安装：

```sh
python3 -m pip install --user -r tools/requirements-format.txt
```

在项目根目录执行：

```sh
python3 tools/format.py          # 格式化 include/、labs/、scratch/ 中的 C/C++ 文件
python3 tools/format.py --check  # 只检查，不修改文件；不符合规则时返回非零状态
```

脚本跳过 build 目录，包含新建但尚未提交的源码。默认优先使用 `~/.local/bin/clang-format`，否则从 PATH 查找；也可以通过 `CLANG_FORMAT=/path/to/clang-format` 指定其他安装位置。

VS Code 已配置 C/C++ 保存时自动格式化；右键 **Format Document** 可手动格式化当前文件，**Terminal → Run Task → Format: C++ / Format: Check C++** 可整理或检查全部源码。编辑器使用 `~/.local/bin/clang-format`；如果其他机器安装路径不同，调整 `C_Cpp.clang_format_path`。编辑器设置依据 [VS Code C/C++ 格式化文档](https://code.visualstudio.com/docs/cpp/cpp-ide#_code-formatting)。

## 性能实验用 Release

```sh
cmake --preset release
cmake --build --preset release
ctest --preset release
```

调试用 Debug，计时用 Release。记录编译选项、工作集、线程数、checksum 和多轮结果。Mac 的 ARM64/page size/cache line 与 Linux 服务器可能不同；NUMA、CXL 和 Linux perf 结论要在对应服务器验证。

## Sanitizer 状态

asan（ASan/UBSan）与 tsan presets 保留，默认关闭且互斥。此前本机 ASan 连简单 smoke 程序也无法正常结束，TSan 进程异常退出；这是这次环境的运行时问题，不能据此断言 macOS 不支持这些工具。本次没有重新验证 sanitizer。

日常先用 Debug/Release 检查、LLDB、状态不变量和边界测试。需要动态内存/竞态检测时，在 sanitizer 运行正常的环境使用对应 preset；重复运行成功和 sanitizer 未报错都不构成并发正确性证明。

旧面经研究和 CV 契合分析保留在 [原指南](../Desktop/record/cv/Quant-Developer-Interview-Roadmap-2026-09-24.md)，本月执行路线以 docs/PLAN.md 为准。
