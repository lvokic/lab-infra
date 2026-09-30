# L04｜iterator、hash/tree 和容器取舍（2h）

材料：R05/R06、R15 __hash_table/__tree。目标是解释选择，不是完整手写红黑树。

## 步骤
1. 给同样一组 key/value，比较 vector、map、unordered_map 的排序/查询/插入接口和复杂度。
2. 用全部返回同一 hash 的测试 hasher 观察冲突退化；记录 bucket_count/load_factor。
3. 用 rehash/reserve 扰动容器，查工作草案契约，分别讨论 iterator 和元素引用的稳定性，不拿失效 iterator 做访问。
4. 画本机 __hash_table 的 bucket/node 关系，找 rehash；画 __tree 的节点关系。源码名称与布局是实现细节。
5. 写表：顺序扫描、随机查询、有序 range query、频繁中间插入、地址稳定需求，各选什么结构并说明反例。

验收：平均与最坏复杂度不混淆；复杂度相同仍能讨论分配、局部性和尾延迟；能解释 deque 不是一种固定标准规定的内存布局。
