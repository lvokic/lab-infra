# L08｜OS、地址翻译和首次触碰（4h）

材料：R09/R10；本机 man mmap/getrusage/sysconf。

## 步骤
1. 画一个进程的代码/数据/堆/映射区域与两个线程的栈和寄存器，回答共享 fd 与共享地址空间。
2. 读取 sysconf(_SC_PAGESIZE)，用 mmap 映射可写匿名区域；一次只访问每页一个位置。
3. 比较分配映射、第一次触碰、第二次触碰的时间与可获得的 getrusage 信息。
4. 基线规模从几十 MiB 起，记录实际访问量；避免把系统推到交换压力才开始观察。
5. 画 VA→TLB→页表→物理页；另画权限故障/非驻留页的处理路径。
6. 解释 syscall 和线程切换的区别；把 CXL 查询的数据访问与线程状态画出来。

## 验收与测量边界
- 不假定本机 page size 是 4 KiB，不把首次触碰的计时差异当独立 TLB miss 证明。
- macOS 计数接口与 Linux 不同；若 getrusage 某计数不提供有效证据，就记录限制。
- 这里没有重建 MMU；TLB/page-fault 机制来自材料，代码仅验证局部现象。
- huge pages、NUMA、perf counters 在你的 Linux/CXL 服务器另测；不在 Mac 上声称复现了这些服务器特性。
