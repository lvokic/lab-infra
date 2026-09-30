# L07｜atomic 发布和 happens-before（2h）

材料：R07、R13 atomics.order。目标是一个受限发布协议，暂不实现无锁队列。

## 步骤
1. 普通 payload，由 writer 写一次；ready.store(true, release)。
2. reader acquire-load 到 true 后读取 payload；画出 sequenced-before、synchronizes-with、happens-before。
3. 固定“写一次、读一次、join 后结束”的协议，解释 acquire 必须读到对应 release（或相关序列）才建立关系。
4. 将 ready 改成 relaxed 的版本只用于分析；普通 payload 的未同步读写可构成 UB，不用输出“看起来对”来证明安全。
5. 增加第二次更新，解释为什么一个反复切换的 bool 不足以表达完整复用/消费协议。

验收：知道 atomic 不自动让关联数据和复合不变量安全；知道 CPU ISA 的常见行为不替代 C++ 语义；能说明等待自旋的 CPU 成本。耗时对比和 SPSC ring buffer 都是选做。
