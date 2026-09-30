# L09｜局部性、AoS/SoA 与访问依赖（4h）

材料：R10 Memory Hierarchy/Cache Memories。使用 Release；Debug 只排错。

## 步骤
1. 在同一组值上做顺序遍历与固定随机 permutation 遍历，保证 checksum 相等。
2. 比较固定 stride 与依赖式 pointer chasing，解释访存并行与预取为何不同。
3. 定义包含多个字段的记录：只访问一个字段，比较 AoS/SoA；记录元素字节数和访问字段。
4. 扫工作集大小；每个条件多次测量，先 warmup，交替测试顺序，报告分布或中位数。
5. 打印 checksum，阻止计算被完全删除；不要随意使用 volatile 来代替合理 benchmark 设计。
6. 对接 DiSPQ 的列式 layout：哪些数据实际被触碰，哪些不再读？

验收：同时记录数据规模、字节数、编译模式、访问次数和校验。结果不设“必须快 X 倍”门槛；能解释 TLB、cache、prefetch、branch、vectorization 等混杂因素。
