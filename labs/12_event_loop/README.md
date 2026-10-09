# L12｜非阻塞 poll、连接状态与背压（4h）

L11 的阻塞发送循环可以等待一个连接，但事件线程必须继续服务其他连接。本节先观察内核 readiness，再实现有界输出队列与单轮 `poll` 驱动；最后接入 L11 parser 和 L06 业务队列。新增观察程序和独立检查，核心实现全部留在 TODO 中。

先完成 L11 parser 的独立检查。这里的基础检查使用原始字节，故不会因为 L11 尚未完成就无法逐步实现 I/O 层。四小时预算覆盖基础层；完整多连接业务集成可再安排一次练习。

## 1. 定点阅读（25 min）

| 材料 | 阅读位置 | 你需要得到的结论 |
|---|---|---|
| [poll(2)](https://man7.org/linux/man-pages/man2/poll.2.html) | `events/revents`、POLLIN/POLLOUT/POLLHUP/POLLERR/POLLNVAL | readiness 是可尝试 I/O，不是一条消息已经完整；HUP 可能伴随未读字节 |
| [fcntl(2)](https://man7.org/linux/man-pages/man2/fcntl.2.html)、[文件状态标志](https://man7.org/linux/man-pages/man2/F_GETFL.2const.html) | F_GETFL、F_SETFL、O_NONBLOCK | 设置非阻塞前保留原来的标志 |
| [recv(2)](https://man7.org/linux/man-pages/man2/recv.2.html)、[send(2)](https://man7.org/linux/man-pages/man2/send.2.html) | EINTR、EAGAIN/EWOULDBLOCK、返回字节数 | 一次 dispatch 能走多步，也可能暂时不能继续 |
| [shutdown(2)](https://man7.org/linux/man-pages/man2/shutdown.2.html) | SHUT_WR | 对端停止发送，不代表它不能继续接收回复 |

先使用 level-triggered `poll`。epoll/kqueue、edge-triggered 公平调度和唤醒 fd 在基础检查通过后再读，不把它们混入第一次实现。

## 2. 文件与命令

| 文件 | 用途 |
|---|---|
| `observe.cpp` | 已完成的内核观察；不依赖你的实现 |
| `event_connection.hpp` | 你的 TODO：`OutboundBuffer`、`PollConnection`、`poll_once` |
| `event_loop_test.cpp` | 纯状态检查和真实 nonblocking socketpair 检查，RAII/建连支撑已完成 |

```bash
cmake --preset debug &&
cmake --build --preset debug --target poll_readiness event_loop_test --parallel 2 &&
./build/debug/poll_readiness &&
./build/debug/event_loop_test --help
```

逐项实现：

```bash
timeout 10s ./build/debug/event_loop_test output_partial
timeout 10s ./build/debug/event_loop_test interest
timeout 10s ./build/debug/event_loop_test read_limit
timeout 10s ./build/debug/event_loop_test all
```

不带参数等同于 `all`；成功 0，检查失败 1，未知参数 2。未实现时报告 TODO 是预期结果。观察程序加入 CTest；手写练习不加入默认 CTest，防止你只完成部分接口时影响其他 lab 的观察验收。

## 3. A：观察三种内核行为（25 min）

观察程序只在本机建立非阻塞 socketpair，输出三组结果：

```text
[A readiness] empty_would_block=1 readable_after_send=1 bytes=3
[B half-close] first_recv=3 next_recv=0 reply_bytes=5
[C backpressure] requested=65536 sent=<实际部分字节数> next_would_block=1 writable=0
```

`sent` 由内核缓冲策略决定，不要求固定数值。该观察是 Linux 本地 socketpair 实验，不能用一次数值推导其他协议栈的缓冲大小。

先解释：

1. A：空 socket 的 `recv` 为什么立刻返回 EAGAIN，而不是等待？向对端写入后 poll 为什么变为可读？
2. B：为什么第一次收到 3 字节，下一次才收到 EOF？对端 `SHUT_WR` 后为什么还能收到 5 字节回复？
3. C：接收方不读取时，为何请求 65536 字节只发送一部分？此时应等待什么事件，而不是立即重试一万次？

在纸上画“用户输出队列 → 内核发送缓冲 → 对端读取”。区分这三处容量；内核 buffer 有界，并不能替你限制用户态输出队列。

## 4. B：有界 OutboundBuffer（45 min）

这个类不调用 socket，只负责保存尚未发送的字节。实现顺序是 `pending_bytes`/`front` → `try_enqueue` → `consume`。

| 接口 | 契约 |
|---|---|
| `try_enqueue(bytes)` | 复制并接纳整个输入，超过尚未发送字节上限则返回 false，不得插入半段 |
| 空输入 | 成功，不生成空队列节点 |
| `front()` | 首段尚未发送的 view，队列为空时为空 view |
| `consume(n)` | 按顺序确认发送的 n 字节，可以跨段；0 合法 |
| `consume(n > pending)` | 抛 out_of_range，状态不变 |
| `pending_bytes()` | 所有尚未发送字节之和 |

画三个快照：排入 `abc`、`def`；发送 2 字节；再发送 2 字节。说明为什么首段 offset 和总 pending 不是同一个数字。

通过 `output_empty`、`output_partial`、`output_limit`、`output_bad_consume` 四项。

容量按“尚未发送的字节”计；已经发送的首段前缀可能暂时留在 string 里。若要严格限制分配内存，可以额外压缩首段或换固定块池，这不是本接口测试保证。零输出容量合法，只能接纳空输入。

容量判断也要避免 `pending + bytes.size()` 的 unsigned 溢出；先建立 `pending <= limit` 的不变量，再比较剩余容量。

## 5. C：连接状态和兴趣事件（30 min）

`PollConnection` **借用 fd**，不会 close；测试用 RAII 负责关闭。fd 构造前已设为 O_NONBLOCK，连接所有字段只由一个事件线程访问，不需要再加 mutex。

先不写系统调用，列出这些状态的含义：

| 字段 | 含义 |
|---|---|
| `received_` / `input_limit_` | 业务还没有取走的输入及上限 |
| `output_` | 已接纳、尚未发送的输出 |
| `paused_` | 业务主动背压，暂停读取 |
| `read_eof_` | recv 实际返回了 0，以后不会再有新输入 |
| `failed_` | 致命错误，不再处理这个 fd |

`wanted_events()` 必须符合：

- 未失败、未 EOF、未暂停且输入有容量时才有 POLLIN。
- 有待发送字节时才有 POLLOUT；暂停读不影响写。
- `failed` 或当前 `done()` 为 true 时为 0。
- `done()` 是 failed，或“EOF、输入已取走、输出已发完”同时成立。

`input_limit == 0` 必须抛 invalid_argument，否则连接无法接收任何数据。`take_received()` 移走本批输入并腾出容量；`queue_output()` failed 后拒绝，读 EOF 后仍可接纳回复。

**业务顺序：取输入 → 解析/处理 → 排入回复 → 最后判断 done 并回收 fd。**`take_received` 后尚未排入回复时，done 可能暂时为 true；不能在业务处理前提前 close。fd 关闭后不得再调用连接方法或排入新输出。

通过 `interest`、`zero_input`；随后进入实际 I/O。

## 6. D：dispatch 与单轮 poll（1h）

在 `dispatch(revents)` 中按下列契约处理，不需要复制观察程序作为完整事件循环：

1. `POLLERR/POLLNVAL` 标记 failed；不再订阅事件。这个基线不尝试恢复连接。
2. `POLLIN` 或 `POLLHUP` 出现，且读允许、输入仍有容量时，尝试 recv。每次请求长度不得超过剩余输入容量。
3. 正数接入输入；EINTR 保留状态并重试；EAGAIN/EWOULDBLOCK 本次读处理结束；0 标记 read EOF；其他错误标记 failed。
4. 输入达到上限立刻停止读。不要为了“drain 到 EAGAIN”突破内存限制；腾出空间后再读。
5. 有 POLLOUT 且输出未空，尝试发送首段 view。只按返回正数调用 consume；EINTR 重试，EAGAIN 留到后续 readiness，其他错误或正长度返回 0 标记 failed。
6. Linux 使用 MSG_NOSIGNAL。POLLHUP 自身不等于“可以扔掉输入和输出”；但如果实际 send 报错则进入 failed。
7. 暂停读时，即使直接传入过时的 POLLIN，也不能继续消耗输入；过时 readiness 下的 EAGAIN 是正常情况。

`poll_once(connections, timeout_ms)` 只做一轮：根据 wanted_events 建临时 pollfd，跳过 done/无兴趣项，调用 poll，再将非零 revents 交给对应连接。返回被 dispatch 的连接数；没有活动项直接返回 0。errno 为 EINTR 时重试，其他 poll 错误抛 system_error。测试参数为非负 timeout；这里允许 EINTR 后重新使用相对 timeout，严格总截止时间是选做。

通过以下真实 socketpair 检查：

| case | 实际验证 |
|---|---|
| `read_limit` | 六字节输入、容量四字节：前四字节进入用户 buffer，后两字节仍留内核 |
| `would_block` | 直接给过时 POLLIN，空 socket 返回 EAGAIN 后连接仍能继续 |
| `pause_and_resume` | 读暂停期间照常发送输出；恢复后收到此前保留的输入 |
| `half_close` | 输入与 EOF 一起处理，业务取出 request 后排入 reply，排空再结束 |
| `partial_send` | 缩小内核发送 buffer，64 KiB 输出实际部分发送/EAGAIN，后续排空且字节无重复丢失 |
| `two_connections` | 一个主动暂停读的连接不妨碍另一个进度 |
| `fatal_event` | 注入 POLLNVAL，连接退出活动集合并拒绝新输出 |

共 13 项检查。`two_connections` 是最小隔离检查，**不是完整慢客户端压力验收**；EINTR、真实 POLLERR/EPIPE、内存分配失败和调度公平性没有自动覆盖。不要以“检查都通过”宣称这些路径都实际发生过。

## 7. E：接入协议和业务队列（另安排 45–60 min）

基础层通过后，在 `scratch/` 创建组合入口，复用本节接口和 L11 的 FrameDecoder：

这一版继续使用 L06 的整数队列：把 payload 固定为十进制 int 文本，例如 `42`、`-7`。
业务层用 from_chars 检查整个 payload 和范围，拒绝格式错误，再将 int 入队；回复采用同一格式。
L11 parser 仍允许任意二进制帧，但此组合业务只接受上述整数文本。
若需要直接排队任意 string，先另外定义字符串队列接口，不把它直接传给现有 push(int)。

1. 每轮取出 received bytes，交给对应连接的 parser；不要把 recv 返回长度当帧长度。
2. read EOF 后，先将最后一批 bytes 交 parser，再调用 finish；半帧按协议错误收尾。
3. 完整消息提交到有界业务队列。事件线程不能调用可能阻塞的 L06 push；为队列补 `try_push`，在同一次持锁操作中检查容量并提交。
4. 提交失败时保留这一条待提交消息，暂停该连接读取。此前同一次 feed 产生的其他消息也要纳入有界 pending 管理，不能无界追加。
5. 队列腾出容量后重试 pending；成功且用户输入有空间才恢复读取。基础版本可以用有界 poll timeout 定期重查，记录延迟；更进一步再用 pipe/eventfd 唤醒。
6. 回复编码成 L11 帧，排入有界输出队列。输出拒绝时保留业务结果或暂停生成，不丢弃半条回复。
7. 对连接设置输入、待提交消息、输出上限，且每个 frame 上限与 parser 一致。设置每轮处理预算，避免忙连接独占事件线程。

这一组合入口**目前由你自行编写，没有预置完整集成检查**。具体验收步骤：同进程建立两对 socketpair，一对暂不读取回复，另一对继续交互；用计数或 latch 控制慢端恢复，不用 sleep 证明顺序；确认快端完成、慢端恢复后全部回复按序收到。关闭请求方的写方向，确认服务方仍能排空最后回复。所有 fd/线程最后收尾，不使用固定端口或公网监听。

避免跨线程随意 close 事件线程持有的 fd；向事件线程提交关闭请求，由 fd 所有者完成清理。否则 fd 编号重用会让旧状态操作到新的连接。

## 8. 最终验收与进阶

基础交付：观察三组输出解释、输出队列快照图、连接状态表、13 项检查通过。综合交付：完整帧顺序、慢端/快端交错记录、各 buffer 上限以及关闭策略。

```bash
cmake --preset asan &&
cmake --build --preset asan --target poll_readiness event_loop_test --parallel 2 &&
./build/asan/poll_readiness &&
timeout 10s ./build/asan/event_loop_test all
```

不要一直订阅 POLLOUT，也不要把 EAGAIN 当循环重试信号。跨线程业务队列可用 TSAN 检查，单线程的原始字节基础层主要用 ASAN/UBSAN 和边界检查。

选做顺序：可注入 recv/send 适配器以确定性验证 EINTR → 严格 poll deadline → 队列恢复唤醒 fd → 多连接公平性 → epoll/kqueue。达到基础和综合交付就可以进入下一阶段，不必先做完整生产网络框架。
