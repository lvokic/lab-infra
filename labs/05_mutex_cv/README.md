# L05｜mutex/CV 的机制和正确交错（4h）

材料：R07/R08/R13/R14。先运行 cv_handshake，再自己扩展。

## 步骤
1. 画 ready/payload/锁的关联；指出哪条关系来自 mutex，哪条来自 thread.join。
2. 在状态更新前做额外 notify；证明 ready=false 时 reader 必须继续等待。额外 notify 是测试扰动，不等于强制制造语言允许的虚假唤醒。
3. 交换“线程开始等待”和“状态更新”的启动时机，使用 latch/barrier 或独立同步，不用 sleep 保障正确性。
4. 写两个消费者争一个资源的交错：被唤醒、等待锁、条件已被另一个消费者改变。
5. 用 wait_until 固定 deadline，讨论重复 wait_for 重置超时的风险。
6. 比较 lock_guard/unique_lock/scoped_lock；讨论 Linux futex 可能参与等待与 macOS 实现差异，不把 API 直接等同于某个 OS 原语。

## 验收
- 准确讲出 wait 的 unlock+block 和 wake 后 relock。
- 条件在同一锁保护下检查和更新；通知本身不保存一个持久“消息”。
- 不要求每次额外通知都产生实际 wake，也不对线程调度顺序做固定断言。
- “去掉锁/while”的版本用交错分析或独立错误实验，不加入正常 CTest。
- 能解释为什么 notify 前后解锁通常都可以写对，并讨论资源生命周期和性能限制。

追问：把 ready 换成 atomic 是否就不需要 mutex？两个不同条件共用一个 CV 会有何通知问题？
