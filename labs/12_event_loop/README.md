# L12｜nonblocking、poll 与背压（4h）

材料：R12 Blocking/poll；R10 System Level I/O；本机 man fcntl/poll/kqueue。先完成 L11 parser 和 L06 队列基线。

## 步骤
1. 对一个连接设置 O_NONBLOCK；将读写剩余数据放入每连接 buffer。
2. 使用 poll 获取 readiness；读取直到 EAGAIN/EWOULDBLOCK，EINTR 处理后重试。
3. 有未发送数据时才订阅 POLLOUT；记录写 offset，防止 busy loop。
4. EOF/POLLHUP 后处理已收到的数据，并决定半帧/写缓冲的关闭策略；不在 fd 关闭后复用旧状态。
5. 接入有界队列和一个慢消费者。事件线程不能调用 L06 中可能阻塞的 push；扩展一个在锁内检查容量的 try_push，失败时保留待提交消息并暂停读。不要用“先查 size，再 push”代替原子提交。读buffer、待提交消息和写buffer都设上限。
6. 容量恢复后重试提交并重新启用读事件。基线可用有界的 poll timeout 周期重查，记录由此增加的响应延迟；消费者通过 pipe 唤醒事件循环属于选做。不要在用户态空转，也不要跨线程随意 close 事件线程持有的 fd。
7. 画 readiness 在 macOS poll/kqueue 和 Linux epoll 中的位置，实际实现先用 poll。

## 验收
一个慢连接不阻塞整个事件循环；所有 buffer/queue 有上界；就绪不等于完整消息或未来调用永不返回 EAGAIN；结束时 fd 和线程都有收尾协议。

只要求一个小型可验证原型。多个连接的公平性、edge-triggered drain、完整重连恢复是选做，超过预算先暂停。
