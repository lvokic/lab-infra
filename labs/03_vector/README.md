# L03｜vector 底层生命周期与 MiniVector（6h）

材料：R04/R05、R13 vector.capacity/modifiers、R15 vector。目标：把内存容量和活对象数量分开。

## 第一阶段：标准容器观察（约 1.5h）
- 预测 observe.cpp，运行 vector_lifetime，解释 reserve/resize/clear 的日志。
- 改成可能抛异常的移动构造且可拷贝的 Tracked，记录实现选择。不要把观测到的倍增因子当标准要求。
- 列 reserve/push_back/insert/erase/clear 的元素引用与 end iterator 失效条件。
- 不解引用已经失效的指针。地址日志在重分配前后分别输出。

## 第二阶段：受限 MiniVector（约 3.5h）
接口：size/capacity/reserve/push_back/clear/operator[]。先 int，再支持可拷贝或 noexcept 可移动的 Tracked。
- 用 allocator 分配未构造 storage，用 construct_at/destroy_at 管理 [0,size)。
- 始终维护 0 <= size <= capacity；释放前先销毁活元素。
- 容器本身实现资源转移的 move；copy 可以显式删除并说明限制。
- reserve 扩容先准备新 storage，成功后提交；为你支持的类型明确异常保证。
- 不用 new T[capacity] 伪装成原始 storage，它会构造 capacity 个 T。
- 可抛异常且不可复制的 move-only 元素，以及完整 allocator propagation 属于选做；不声称实现与 std::vector 完全等价。

## 第三阶段：验证（约 1h）
- 空容器、capacity 1、连续增长、clear 后重用、move 后两容器析构。
- 与 std::vector 对同一组有固定随机种子的 push/reserve/clear 操作比较值与 size。
- 活对象计数与 size 一致；退出后归零。
- 用 copy 可抛异常的测试类型注入一次扩容失败，检查已构造新元素清理和原状态（如果你的契约承诺强保证）。
- 讨论 push_back(v[0]) 在扩容时的参数别名问题：若支持它，应先保全输入再让旧元素失效；若基线不支持，必须明确限制，不声称覆盖 std::vector 的全部语义。

追问：为什么摊还 O(1) 不等于低延迟？移动整个 vector 与移动其中元素有什么区别？为什么 reserve 不该在每次 push 前调用？
