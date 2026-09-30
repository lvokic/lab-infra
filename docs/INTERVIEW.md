# 闭卷问题与追问链

这些是基于你的学习重点、简历和之前面经调研制定的模拟问题，不能称为当前 Optiver 真题。先回答主问，再由同学/自己随机抽追问；每题 2 分钟解释，难题追加 3 分钟画图或代码。

## C++ 对象模型、所有权与 STL

| 编号/优先级 | 主问 | 要能应对的追问 |
|---|---|---|
| C01 P0 | 内存分配和对象生命周期有什么区别？ | reserve/resize 分别做什么？raw storage 怎样构造非默认构造类型？ |
| C02 P0 | RAII 怎样处理异常和提前 return？ | 一个管理 fd 的类可否复制？移动后谁负责 close？ |
| C03 P0 | std::move 做了什么？ | const 对象使用 std::move 会怎样？右值引用有名字后作为表达式是什么类别？ |
| C04 P0 | 拷贝、移动和 Rule of Zero/Five 如何关联？ | 用户声明析构会影响什么？移动赋值已有资源怎么办？ |
| C05 P0 | virtual 调用怎样选择函数？ | 构造期间的动态派发？override/final 的意义？优化器能否去虚化？ |
| C06 P0 | 为什么多态基类常需要虚析构？ | protected 非虚析构适用于什么设计？对象切片发生在哪一步？ |
| C07 P0 | vptr/vtable 可能放什么信息？ | 哪些是 ABI 实现细节？多继承为什么有 this 调整？怎么用编译器核实？ |
| C08 P0 | unique_ptr/shared_ptr/weak_ptr 怎么表达所有权？ | 对象和控制块什么时候销毁？make_shared 的分配有什么特点？ |
| C09 P0 | shared_ptr 是线程安全的吗？ | 不同 shared_ptr 实例共享计数、同一个实例被改写、指向对象被修改，分别怎样同步？ |
| C10 P0 | vector 的 size/capacity 和扩容做什么？ | clear 是否释放容量？shrink_to_fit 是保证吗？ |
| C11 P0 | vector 的 iterator/reference 何时失效？ | reserve、push_back、erase 的不同情形；end iterator 的特殊点？ |
| C12 P0 | noexcept 为何影响元素搬迁？ | copy 可用 vs throwing move-only 的异常保证；move vector 和 move 每个元素有什么区别？ |
| C13 P1 | unordered_map 是 O(1) 吗？ | rehash、冲突、load factor、平均/最坏、tail latency？ |
| C14 P1 | map/list/deque/vector 怎样选？ | 有序查询、局部性、分配开销、地址稳定性、插入/擦除位置？ |
| C15 P1 | string_view/span 为什么可能悬空？ | 谁拥有底层内存？容器扩容或临时对象结束以后呢？ |

练习：画 vector 的三段区间、shared_ptr 的对象/控制块图、virtual 调用路径；优先用图和自己的小代码验证。暂不把复杂模板/协程加入题单。

## mutex、condition_variable 与并发

| 编号/优先级 | 主问 | 要能应对的追问 |
|---|---|---|
| T01 P0 | data race 和逻辑 race 有什么区别？ | atomic 计数正确时，一个复合状态为何仍错？ |
| T02 P0 | mutex 究竟保护什么？ | 锁只保护写，读可以不锁吗？锁的范围如何界定？ |
| T03 P0 | lock_guard/unique_lock/scoped_lock 怎么选？ | 为什么 CV wait 需要 unique_lock？多个锁怎样获取？ |
| T04 P0 | CV wait 的原子步骤是什么？ | 检查条件与睡眠之间的空隙怎样处理？返回时持锁吗？ |
| T05 P0 | 为什么带谓词或 while 等待？ | 额外通知、虚假唤醒、多个消费者抢走状态？ |
| T06 P0 | notify 在 wait 之前会怎样？ | 通知不是持久化消息；持久状态怎样保证无需等待？ |
| T07 P0 | notify_one/notify_all 何时选？ | 关闭两个谓词队列为什么两边都要唤醒？先 unlock 再 notify 是否总是更快？ |
| T08 P0 | 有界队列怎样关闭？ | drain policy、push 拒绝、满/空阻塞者、幂等 close、线程 join 与对象析构？ |
| T09 P0 | 怎么证明队列不丢不重？ | 有唯一 ID 的压力验证、记录入队/出队、线性化点、允许的顺序？ |
| T10 P1 | 死锁有哪些条件？ | 一致的锁顺序、scoped_lock、阻塞调用持锁的问题？ |
| T11 P1 | acquire/release 怎样发布普通数据？ | acquire 必须读到哪个写入？为何重复使用 ready bool 容易破坏单次发布协议？ |
| T12 P1 | mutex、spinlock、atomic 怎样取舍？ | 临界区时长、CPU 占用、争用、调度和复合不变量？ |

不要把“压力运行没出错”当成并发证明，也不要只凭 TSan 无报告判定程序正确。对错误代码先给交错和同步关系。

## OS、内存与体系结构

| 编号/优先级 | 主问 | 要能应对的追问 |
|---|---|---|
| S01 P0 | 进程和线程共享/独有的东西？ | 地址空间、fd、栈、寄存器、隔离和通信？ |
| S02 P0 | 系统调用是不是一次线程上下文切换？ | 用户/内核模式转换、阻塞和调度分别是什么？ |
| S03 P0 | VA 到 PA 怎么走？ | 页表、权限、TLB miss、page fault 的区别？ |
| S04 P0 | malloc/new/mmap 为什么不等于立即获得物理页？ | allocator、映射、首次触碰、RSS、页缓存？ |
| S05 P0 | cache line 和 locality 为什么改变性能？ | 连续访问、随机依赖、工作集、预取和向量化？ |
| S06 P0 | false sharing 与真正共享？ | 如何确定地址/对齐？padding 为什么可能无益甚至更慢？ |
| S07 P0 | 同样 O(n) 为什么耗时不同？ | layout、访存、分配、分支和并行；怎样排除编译优化和测量噪声？ |
| S08 P1 | huge pages/NUMA 对程序可能有什么影响？ | TLB 覆盖、放置、调度和分配条件；你自己的服务器证据？ |
| S09 P0/CV | CXL/DiSPQ 的瓶颈是什么？ | 测量如何区分 latency/bandwidth/compute，batching 为什么可能改变 P99？ |
| S10 P1 | release/acquire 和 ARM/x86 重排是什么关系？ | 用 C++ memory model 推理；不能用某个 ISA 的经验代替语言同步。 |

## 网络和 I/O

| 编号/优先级 | 主问 | 要能应对的追问 |
|---|---|---|
| N01 P0 | TCP 和 UDP 的应用语义有什么区别？ | stream/datagram、可靠性/顺序、时效性与恢复策略？ |
| N02 P0 | 一次 recv 为何不能当一条消息？ | 分片/合并、length prefix、缓冲上限、网络字节序？ |
| N03 P0 | send 是否保证发送所有字节？ | 部分返回、EINTR、EAGAIN、写缓冲和重试？ |
| N04 P0 | recv=0 是什么？ | 对端关闭写方向、连接中尚未处理的完整/不完整帧？ |
| N05 P1 | nonblocking 与 poll 的关系？ | 就绪通知不等于完整消息；事件后 read 仍返回 EAGAIN 怎么办？ |
| N06 P0 | TCP 流控、拥塞控制、应用背压如何区别？ | 接收窗口、网络负载、应用队列上限分别在何处？ |
| N07 P0 | 慢消费者怎样影响系统？ | 内存上界、丢弃策略、暂停读取、超时、尾延迟？ |
| N08 P1 | UDP 行情缺失/重复/乱序怎么处理？ | sequence 作用域、buffer 上限、超时/快照恢复？ |
| N09 P1 | poll、kqueue、epoll 在哪个平台？ | 共同的 readiness 模型，level/edge 机制和 drain/read 状态？ |

## 三条综合模拟链

1. **MiniVector，45 分钟。** 从整数版 API 解释到泛型生命周期、扩容、move/noexcept、异常保证；最后问缓存和分配开销。
2. **有界队列，45 分钟。** 澄清 close/drain → 谓词和锁 → 满/空边界 → 多线程正确性证据 → contention/backpressure。
3. **行情读取路径，45 分钟。** socket/read buffer → framing → bounded queue → state update；追加乱序/EOF/慢消费者，再问优化怎样影响 P99。

每次录音后只选最差三项，第二天改解释或补实验。每周另用 2 小时维持算法题限时能力，不在本目录提供基础题答案。
