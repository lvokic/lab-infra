# 视频与材料：看什么、怎么用、对应哪个实验

检索核实日期：2026-09-30。以下链接优先来自讲者、会议、课程、作者或标准工作草案；推荐顺序是针对你的任务作出的判断。预算是你阅读、暂停和做笔记的时间估算，不是已核实的视频精确时长。

每个主题选一个主入口，随后进入实验。视频的角色是建立机制图；规范用于核对契约；源码用于理解本机实现。英文字幕可辅助，但字幕翻译中的 ownership/lifetime/memory order 要回到原术语检查。

## 最少先打开这五组

| 方向 | 首选 | 对应实验 |
|---|---|---|
| 对象模型 | R01 Class Layout 材料 + R02 RAII 视频 | L01/L02 |
| STL 与智能指针 | R03 Smart Pointers 视频 + R05 Containers 视频 + R15 本机源码 | L02/L03/L04 |
| mutex/CV | R07 C++ Concurrency 视频 + R08 OSTEP Condition Variables | L05/L06 |
| OS/内存体系结构 | R09 OSTEP paging/TLB + R10 CMU 指定讲次 | L08/L09/L10 |
| 网络/I/O | R11 作者 Transport Layer 视频 + R12 Beej 编程章节 | L11/L12 |

## R01｜对象布局和动态派发：必读材料

- Steve Dewhurst，CppCon 2020：[Back to Basics: Class Layout 官方 slides](https://github.com/CppCon/CppCon2020/blob/main/Presentations/back_to_basics_class_layout/back_to_basics_class_layout__steve_dewhurst__cppcon_2020.pdf)。
- 只定点阅读 data member layout、alignment/padding、virtual call、继承相关部分。先解释单继承，再讨论多继承为什么需要指针调整。
- 分配约 45–60 分钟，随后做 L01。用编译器 dump 和 LLDB 核对本机布局。
- 选读：[Itanium C++ ABI §2.5 Virtual Table Layout](https://itanium-cxx-abi.github.io/cxx-abi/abi.html#vtable)。它是 ABI 机制资料；C++ 标准不规定统一 vptr/vtable 布局，Apple 平台的实际行为应由本机工具核对。不要靠猜偏移强转调用虚表。

## R02｜RAII 和 special members：必看视频

Arthur O'Dwyer，CppCon 2019：[Back to Basics: RAII and the Rule of Zero](https://www.youtube.com/watch?v=7Qgd9B1KuMQ)。

先问三个问题：资源由谁拥有、什么时候释放、复制/移动改变谁的责任。观看时暂停重写小例子，重点关注资源包装类和组合业务类。预算约 60–90 分钟。对应 L02；看完要能解释为什么一个自管资源类不能直接使用浅拷贝。

## R03｜智能指针底层：必看视频

Arthur O'Dwyer，CppCon 2019：[Back to Basics: Smart Pointers](https://www.youtube.com/watch?v=xGDLkt-jBJ4)。

重点：unique ownership、shared control block、weak ownership、make_shared/make_unique。enable_shared_from_this 作为后续选读，不挤占基本所有权实验。预算约 60–90 分钟。对应 L02 和 R15 源码路线。用自己的图解释对象销毁与控制块销毁为什么可能不同时发生。

## R04｜移动语义：必读缺口材料

David Olsen，CppCon 2020：[Back to Basics: Move Semantics 官方 slides](https://github.com/CppCon/CppCon2020/blob/main/Presentations/back_to_basics_move_semantics/back_to_basics_move_semantics__david_olsen__cppcon_2020.pdf)。

围绕 rvalue reference、std::move、隐式生成、const、noexcept 和 moved-from state 阅读；不要求记所有模板推导规则。预算约 45–60 分钟。用计数型类型验证“转换”和“被选中的构造函数”是两个问题。对应 L02/L03。

## R05｜容器选型：必看/按熟悉程度跳看

Rainer Grimm，CppCon 2022：[Back to Basics: Standard Library Containers](https://www.youtube.com/watch?v=ZMUKa2kWtTk)。

重点选 vector、deque/list、map/unordered_map 的部分。预算 45–75 分钟。做完要给出按查询、更新、顺序性、地址稳定性选择容器的表。底层对象构造和异常路径再用 R13/R15 核实；这个视频用于选型，不替代源码实验。对应 L03/L04。

## R06｜iterator：定点视频

Nicolai Josuttis，CppCon 2023：[Back to Basics: Iterators in C++](https://www.youtube.com/watch?v=26aW6aBVpk0)。

看 iterator category、容器/算法接口与典型 corner cases；ranges/views 是拓展内容。预算 30–45 分钟选看。复习的重点是失效规则与要求，不是给每个 iterator 背实现。对应 L03/L04。

## R07｜mutex、data race、CV：主视频

David Olsen，CppCon 2023：[Back to Basics: C++ Concurrency](https://www.youtube.com/watch?v=8rEGu20Uw4g)。

依次看 thread lifetime、data race、mutex/RAII locking、atomic、condition_variable。预算 75–90 分钟含暂停。对应 L05/L06/L07，暂停画两线程交错。

## R08｜并发等待的机制：必读章节

[OSTEP 作者主页](https://pages.cs.wisc.edu/~remzi/OSTEP/)：
- [Locks](https://pages.cs.wisc.edu/~remzi/OSTEP/threads-locks.pdf)：理解等待、争用、调度与锁实现的分层。
- [Condition Variables](https://pages.cs.wisc.edu/~remzi/OSTEP/threads-cv.pdf)：重点 join 等待、producer/consumer、循环检查条件。
- [Common Concurrency Problems](https://pages.cs.wisc.edu/~remzi/OSTEP/threads-bugs.pdf)：用于构造交错反例。

合计预算约 90–120 分钟定点阅读。示例多用 pthread，练习时转换成 std::mutex/unique_lock/condition_variable；两层 API 不能简单逐字照搬。对应 L05/L06。

## R09｜OS 和虚拟内存：章节主线

[OSTEP 作者主页](https://pages.cs.wisc.edu/~remzi/OSTEP/)提供章节：
- [Processes](https://pages.cs.wisc.edu/~remzi/OSTEP/cpu-intro.pdf) 与 [Process API](https://pages.cs.wisc.edu/~remzi/OSTEP/cpu-api.pdf)：进程和资源隔离。
- [Address Translation](https://pages.cs.wisc.edu/~remzi/OSTEP/vm-mechanism.pdf)：建立地址转换图。
- [Paging](https://pages.cs.wisc.edu/~remzi/OSTEP/vm-paging.pdf) → [TLBs](https://pages.cs.wisc.edu/~remzi/OSTEP/vm-tlbs.pdf) → [Beyond Physical Memory: Mechanisms](https://pages.cs.wisc.edu/~remzi/OSTEP/vm-beyondphys.pdf)。

预算 3–4 小时分多次阅读；paging/TLB 是主线。对应 L08。问自己：一次 TLB miss 和一次 page fault 分别由谁处理，为什么成本不同？这是阅读后的自拟验收题。

## R10｜体系结构、OS/I/O：CMU 15-213 指定讲次

CMU 官方 [2015 Fall schedule](https://www.cs.cmu.edu/afs/cs/academic/class/15213-f15/www/schedule.html)提供作者讲义、代码以及 Videotaped lectures 入口。按照标题选择：
1. The Memory Hierarchy、Cache Memories。
2. Virtual Memory: Concepts、Virtual Memory: Systems。
3. ECF: Exceptions & Processes。
4. System Level I/O。
5. Network Programming: Part 1/2（与 Beej 内容重复时跳读）。

公开视频入口由课程页链接到 Panopto；自动浏览能打开目录，但未加载出可播放列表，因此不承诺每个视频当前都可免登录播放。优先用同一课程页的公开 PDF/代码，视频能打开再按上述标题选看。

本计划只给这一组 4–5 小时预算，视频与 PDF 二选一，不通看整门课。若有 CS:APP 第 3 版，定点用第 6 章、第 9 章、第 10/11 章作文字补充；x86-64 教学汇编要与本机 ARM64 区分。对应 L08/L09/L10/L12。

## R11｜网络理论：作者的公开短视频

Kurose/Ross 作者页面：
- [Transport Layer 视频与讲义](https://gaia.cs.umass.edu/kurose_ross/videos/3/)：先选 3.1、3.3、3.5；再看 TCP flow control 和 congestion control 对比。
- [Computer Networks and the Internet](https://gaia.cs.umass.edu/kurose_ross/videos/1/)：只补 delay、loss、throughput 的部分。
- [官方 Online Lectures 总入口](https://gaia.cs.umass.edu/kurose_ross/online_lectures.htm)。

这一组预算约 2–3 小时含暂停和问答；不用看完整网络层路由章节。对应 L11/L12。TCP 流控和应用队列背压在不同层，实验时把两者画在不同位置。

## R12｜socket 编程和 I/O：必读作者指南

[Beej's Guide to Network Programming](https://beej.us/guide/bgnet/html/split/)：
- System Calls：send/recv、关闭与 socket 建立流程。
- Slightly Advanced Techniques：Blocking、poll、Handling Partial send、Data Encapsulation。

预算约 90–120 分钟。对应 L11/L12；每读一个片段就转换为 C++ RAII fd 实验。Mac 先用 poll；kqueue 是平台拓展，Linux epoll 用于概念比较和后续服务器复验。操作细节用本机 man recv/send/poll/kqueue 查询。

## R13｜语义契约：用工作草案核对

C++ 工作草案的公开 HTML：
- [vector.capacity](https://eel.is/c++draft/vector.capacity) 与 [vector.modifiers](https://eel.is/c++draft/vector.modifiers)。
- [thread.condition.condvar](https://eel.is/c++draft/thread.condition.condvar)。
- [atomics.order](https://eel.is/c++draft/atomics.order)。

这是持续更新的草案，不是固定 C++20 印刷标准；本项目按 C++20，专注已有的基本语义。每次只核对 1–2 个问题：reserve 是否构造元素、哪些操作失效、wait 返回时锁是否持有、release/acquire 何时同步。不要把当前草案的新接口直接引入练习。

## R14｜工程约束：定点指南

[C++ Core Guidelines](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines)按条目检索：
- C.35/C.67：基类析构和多态复制。
- R.20/R.21：智能指针与所有权选择。
- CP.20/CP.42/CP.50：RAII 锁、带条件等待、状态与锁的关联。

遇到实验疑问再查，预算不另加完整阅读。对应 L01/L02/L05/L06。

## R15｜你这台 Mac 的 libc++：实现资料

先看 [STL_SOURCE_MAP.md](STL_SOURCE_MAP.md)。它已经核对本机 SDK 的 vector、unique_ptr、shared_ptr、hash table 和 tree 文件，给出查找符号的步骤。先写不变量和 API 契约再打开源码；限定每次 30–45 分钟。

官方上游 [LLVM libc++ include](https://github.com/llvm/llvm-project/tree/main/libcxx/include)可作对照。上游 main 与 Apple SDK 版本可能不同；本机观测以 SDK 头文件为准。暂时不深入宏、debug iterator/allocator 的所有分支。

## 不增加资源时怎样补弱项

- 不懂虚表：回到 R01 + L01 的布局和调用链，不新增一门语言课。
- smart_ptr 只会背：画 strong/weak、对象/控制块图，再跑 L02。
- CV 总写错：用 R08 一个 producer/consumer 例子，画三种 interleaving 再写 L06。
- OS 抽象：R09 一个章节配 L08 一个可观察问题。
- 网络抽象：R12 一页 API 文档配 L11 一个分片输入。
