# L07 动手任务与检查对照

先看 [README](README.md) 的原子计数 observer，再实现
[atomic_publication.hpp](atomic_publication.hpp)。核心函数保持 TODO，不提供参考答案。

## 必做步骤

1. 列出 writer 和 reader 分别访问的字段，区分 atomic 与普通对象。
2. 写出单次发布的调用方前提；实现两个接口。
3. 在运行线程之前画同步关系，指出 acquire 必须观察到哪个 release。
4. 分别运行下面的检查，最后运行 all。

| case | 验证内容 | 不能从这项单独推出什么 |
|---|---|---|
| initially_empty | 未发布时返回 nullopt，不去读未发布的数据 | 并发发布正确 |
| writer_first | 发布状态保留，全部字段一致 | release/acquire 必然正确；此项没有并发 |
| reader_first | 首次未发布，之后取得完整 payload | 调度或重试次数固定 |
| concurrent_start | 共同起点后的 writer 与 reader 能交付完整数据 | 某次成功覆盖了所有交错 |
| repeated_reads | 不可变 payload 能被重复读取 | 同一个对象能重新发布 |
| multiple_readers | 两个读者都取得同一份数据 | 一个 reader 能消费或删除它 |
| independent_rounds | 20 个新对象分别发布一次 | 反复切换同一个 ready 安全 |

```sh
cmake --preset debug &&
cmake --build --preset debug --target atomic_publication_test --parallel 2 &&
timeout 15s ./build/debug/atomic_publication_test reader_first
```

每次修改头文件后重新构建。参数支持 all、单项、--help；退出码 0 通过、1 失败、2 参数错误。
外部 timeout 超时默认为 124，它不会为业务接口增加超时语义。

## 需要提交的解释

- writer 写 payload 到 reader 读 payload 的完整关系链，标出读到了哪次原子写。
- 为什么返回 nullopt 的 reader 不能同时读普通 payload。
- 为什么多个 reader 可以复读，而 writer 不能改写第二份。
- observer 的 relaxed 计数为何不能直接作为普通 payload 发布模板。

## 纸上分析与选做

relaxed 发布、先标记后写数据、复用 payload 三个错误变体只写交错和缺失的关系，
不提供可执行版本。不要依靠“本机读到了正确值”判断不存在 UB。

选做：比较 L05 的阻塞等待与当前非阻塞 try_read 的调用方重试成本。
需要 timeout 时使用单个 steady_clock 截止时间，但不要把计时或 yield 当作同步。
在没有明确消费完成确认和对象生命周期协议前，不加 reset。
