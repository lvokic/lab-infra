# 本机 libc++ 源码阅读地图

核对日期：2026-09-30。当前环境为 ARM64 Mac、Apple Clang 17、Xcode SDK 的 libc++。这些链接对应本机文件；切换 Xcode 或更新 SDK 后，请重新用符号定位，行号可能改变。

## 先分清三层

1. C++ 语义契约：失效、生命周期、复杂度和异常保证，查 [R13/R14](RESOURCES.md)。
2. libc++ 实现：具体字段、增长策略、控制块、树和哈希节点，在下面文件里观察。
3. 当前实验结果：特定类型、优化级别和平台的输出，记录在实验笔记中。

实现中的增长倍率、成员布局或一次调用的计数，不自动成为跨平台标准保证。源码也不能证明业务对象的线程安全。

## 入口和阅读问题

| 主题 | 本机文件 | 先追什么问题 |
|---|---|---|
| vector | [__vector/vector.h](/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk/usr/include/c++/v1/__vector/vector.h:396) | 哪些字段表示已分配存储和已构造元素？reserve 怎样迁移和提交？ |
| vector 增长 | [__recommend](/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk/usr/include/c++/v1/__vector/vector.h:886) | 本机的增长选择如何支持摊还复杂度？和标准的最低保证有什么区别？ |
| unique_ptr | [__memory/unique_ptr.h](/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk/usr/include/c++/v1/__memory/unique_ptr.h) | 指针和 deleter 放在哪里？reset/release 和 move 分别改变谁的责任？ |
| shared_ptr | [__memory/shared_ptr.h](/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk/usr/include/c++/v1/__memory/shared_ptr.h:95) | 存储的对象指针与控制块指针有什么区别？make_shared 的分配路径是什么？ |
| 强弱计数 | [__memory/shared_count.h](/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk/usr/include/c++/v1/__memory/shared_count.h:75) | 强引用归零时销毁什么？弱引用生命周期结束后才释放什么？ |
| hash table | [__hash_table](/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk/usr/include/c++/v1/__hash_table) | bucket、node、rehash、load factor 分别关联什么状态？ |
| tree | [__tree](/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk/usr/include/c++/v1/__tree) | 节点链接与遍历关系是什么？为何迭代顺序与插入顺序不同？ |

## 30–45 分钟阅读法

以 vector 为例：
- 先写自己的模型：begin / end / capacity end；已构造区间与仅有存储区间。
- 看 reserve 声明，再找定义和 __swap_out_circular_buffer；画出分配、构造、销毁、提交的顺序。
- 查 __recommend；不要把某个版本的增长因子写进 MiniVector 的测试契约。
- 查扩容时调用的 relocation helper，追到构造或 move/copy 的选择；用 L03 的 noexcept 变化实验交叉验证。
- 记录一个正常路径和一个异常路径。宏和 allocator 边角暂时跳过，碰到影响契约的分支再回来。

以 shared_ptr 为例：
- 从 __shared_ptr_pointer 和 __shared_ptr_emplace 区分独立分配与合并分配。
- 从 __on_zero_shared / __on_zero_shared_weak 画对象和控制块的两个终点。
- 注意本机计数字段并不一定直接等于 use_count：这份 shared_count 使用了偏移计数，查看 use_count 的实现。
- 区分“不同 shared_ptr 实例共享控制块”“同一 shared_ptr 实例被并发修改”“多个线程访问 pointee”。三者的同步要求不同。
- 不实现完整 shared_ptr；用 weak_ptr 的失效、lock 与 deleter 观察生命周期。

## 终端导航

当前 Apple SDK 的实际路径由 xcrun 查询，避免硬编码 Xcode 版本：

```sh
LAB_SDK="$(xcrun --show-sdk-path)"
LAB_LIBCXX="$LAB_SDK/usr/include/c++/v1"
rg -n '__recommend|__swap_out_circular_buffer|reserve\(' "$LAB_LIBCXX/__vector/vector.h"
rg -n '__shared_ptr_pointer|__shared_ptr_emplace|__on_zero_shared' "$LAB_LIBCXX/__memory/shared_ptr.h"
rg -n '__shared_owners_|__shared_weak_owners_|use_count' "$LAB_LIBCXX/__memory/shared_count.h"
rg -n 'release\(|reset\(' "$LAB_LIBCXX/__memory/unique_ptr.h"
rg -n 'rehash|__bucket_list_|__node' "$LAB_LIBCXX/__hash_table"
```

在 VS Code 直接打开上表文件，或从 #include 跳转；这些 SDK 文件只读，练习代码放在本项目 scratch 或 labs 内。源码阅读后填写 [实验记录模板](EXPERIMENT_TEMPLATE.md)，不要用抄实现代替闭卷解释。

官方上游：[LLVM libc++ include](https://github.com/llvm/llvm-project/tree/main/libcxx/include)。上游 main 与 Apple SDK 不同，比较时先确认文件和版本。当前以本机头文件解释本机观察。
