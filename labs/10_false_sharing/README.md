# L10｜共享、false sharing 和布局控制（3h）

材料：R10 Cache Memories；先完成 L09。

## 步骤
1. 从 sysctl -n hw.cachelinesize 获取本机公开值；记录地址/对齐，不能默认 cache line 是 64 B。
2. 比较：同一个 atomic counter；每线程独立 counter 但相邻；每线程独立 counter 且隔开。
3. 各线程做相同次数的 atomic fetch_add(relaxed)，join 后检查总和；relaxed 只表达计数操作，不充当其他数据的同步。
4. 所有条件使用相同操作和线程数，固定运行次数，多轮交替运行；线程启动成本单列。
5. 采用明确的 alignment/spacing，打印地址验证实验布局；不要仅靠类名宣称“padded”。
6. 没测到明显差异时写替代解释：调度、核类型、迭代数、硬件结构、计时噪声、布局。

验收：区分 true sharing 与 false sharing；说明相邻变量是否真的落在同一缓存行；知道计时本身不能完整证明一致性流量。Mac 实验方法可迁移，服务器结论需在目标硬件复验。
