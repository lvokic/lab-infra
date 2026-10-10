# L12 补充练习：epoll 的 LT、ET 与非阻塞处理（3–4h）

这是 Linux 专项练习，接在 poll 基础之后。脚手架不依赖 PollConnection 或 L11 parser，
可以单独构建。核心操作全部留为 TODO，不提供实现答案。

| 文件 | 用途 |
|---|---|
| [epoll_exercises.hpp](epoll_exercises.hpp) | 你实现 EpollSet、非阻塞读循环和写循环 |
| [epoll_exercises_test.cpp](epoll_exercises_test.cpp) | 已完成检查；只用本机 socketpair，无公网或固定端口 |
| [README 第 0 节](README.md#0-从阻塞-io-理解-poll-和-epoll首次接触先读) | 首次接触 readiness 的入口 |

## 1. 先建立模型，不要先写完整服务器（20 min）

poll 每轮提交关注数组；epoll 保留关注集合，你在发生变化时调用 ctl，在每轮调用 wait。
这里的 EpollSet 管理的是“哪些 fd 值值得关注”，不是保存网络消息的队列。

```text
创建 epoll 实例
        ↓
add：登记 fd + 关注事件 + LT/ET
        ↓
wait：返回这一批就绪 fd 和实际事件
        ↓
对就绪 fd 执行非阻塞 I/O，维护业务状态
        ↓
modify：调整关注事件；remove：取消关注
```

| 材料 | 本阶段只读这些内容 |
|---|---|
| [epoll_create1(2)](https://man7.org/linux/man-pages/man2/epoll_create1.2.html) | 返回的实例 fd、EPOLL_CLOEXEC、失败值 |
| [epoll_ctl(2)](https://man7.org/linux/man-pages/man2/epoll_ctl.2.html) | ADD/MOD/DEL、events、data、EPOLLET、EPOLLRDHUP |
| [epoll_wait(2)](https://man7.org/linux/man-pages/man2/epoll_wait.2.html) | 返回数量、maxevents、timeout、EINTR |
| [epoll(7)](https://man7.org/linux/man-pages/man7/epoll.7.html) | LT/ET 示例、非阻塞、EAGAIN、用户态 ready list |
| [recv(2)](https://man7.org/linux/man-pages/man2/recv.2.html)、[send(2)](https://man7.org/linux/man-pages/man2/send.2.html) | 返回字节数、EOF、EAGAIN、EINTR、MSG_NOSIGNAL |

先解释两个结构：epoll_event.events 是事件位；epoll_event.data 是返回给程序的用户标识。
本练习采用 data.fd。不要把 data.ptr 指向可能被 vector 扩容移动的临时对象。
用位与判断事件，因为 IN、RDHUP、HUP 等可能同时出现。

## 2. 文件、运行入口与固定契约

```bash
cmake --preset debug &&
cmake --build --preset debug --target epoll_exercises_test --parallel 2 &&
./build/debug/epoll_exercises_test --help
```

检查支持 `all`、一个 case 名、`--help`。不带参数运行 all；0 通过、1 失败、2 参数错误。
外部 timeout 超时通常为 124；组件本身不会被它自动添加业务超时。
初始版本全部明确报 TODO，这是预期结果，不能把可编译误认为已实现。
这个学员入口不加入默认 CTest，已完成的 poll_readiness 观察仍独立运行。

EpollSet 的约束：

- 实例 fd 由 EpollSet 拥有，析构已负责 close；关注的 socket 是借用，remove 不关闭它。
- 所有方法由同一个事件线程调用。先 remove，再由 owner close socket；不允许其他线程偷关 fd。
- events 只接收 IN、OUT、RDHUP 的任意组合，0 合法。ET 由独立的 trigger 参数指定。
- 错误/挂断事件仍可能在返回的实际事件中出现，不把它们当作用户传入的关注位。
- 负 fd、非法事件位/模式、重复 add 是 invalid_argument；未登记的 modify/remove 是 out_of_range。
- 内核调用失败是 system_error。失败后用户态登记和原有内核关注应保持一致，不先删除旧登记。
- timeout 只支持非负数，max_events 范围 1–64；先校验参数，再处理空集合。
- wait 只返回内核填充的条目，不复制整个预分配数组；不要求事件顺序。
- EINTR 后可重新使用相对 timeout。严格总截止时间、多线程关闭、fd 编号重用保护另作扩展。

## 3. A：实现关注集合和等待（45–60 min）

实现顺序：构造实例 → add → wait → modify → remove。每完成一个接口运行对应检查。
size/contains、实例 fd 析构支撑已经完成；不要修改检查来适应内部表示。

你需要自己决定：校验放在哪里、事件缓冲放在哪里、登记与 ctl 如何保持一致。
对 add 考虑目录分配与内核调用分别失败的路径；不能留下内核关注但用户态没有记录的半成功状态。
用户态登记不是第二套 readiness，只负责描述你已向内核登记的关注关系。

```bash
timeout 15s ./build/debug/epoll_exercises_test instance
timeout 15s ./build/debug/epoll_exercises_test empty_wait
timeout 15s ./build/debug/epoll_exercises_test validation
timeout 15s ./build/debug/epoll_exercises_test registry
timeout 15s ./build/debug/epoll_exercises_test ctl_failure
timeout 15s ./build/debug/epoll_exercises_test modify_interest
timeout 15s ./build/debug/epoll_exercises_test remove_interest
timeout 15s ./build/debug/epoll_exercises_test multiple_fds
timeout 15s ./build/debug/epoll_exercises_test batch_limit
```

检查使用的 timeout 很小，没有 sleep 排顺序。多 fd 的返回顺序不限，max_events=1 不丢弃其他待报告连接。
本练习不注入分配失败和 EINTR；这两条路径仍需解释和代码审查。

## 4. B：用部分读取区分 LT 与 ET（20 min）

同一个本机 socket，一次写入 abcdef，收到事件后只读取 ab，然后再调用 wait：

| 模式 | 此时的输入 | 本检查观察什么 |
|---|---|---|
| LT | 内核仍有 cdef | 仍报告可读，读完后不再报告 |
| ET | 内核仍有 cdef | 在没有新的输入/关闭变化时，本检查不再得到第二次事件；原始 recv 仍能取得 cdef |

```bash
timeout 15s ./build/debug/epoll_exercises_test lt_partial
timeout 15s ./build/debug/epoll_exercises_test et_partial
```

这两项用原始 recv 刻意只读前缀，独立于你尚未实现的 drain 函数。
ET 检查是受控 Linux socketpair 场景，不是说真实程序一次变化只能报告一次，
也不是允许程序假定所有通知都恰好一次。重复/过时就绪下仍需正确处理 EAGAIN。
自己画一次交错：对端发请求后等待回复，服务端只读半条再无限 wait，为何双方可能没有进展？

## 5. C：实现有预算的非阻塞读取（40 min）

实现 epoll_read_available(fd, byte_limit)。返回值包括实际 bytes 和停止原因：

| stop | 含义 | 下一步 |
|---|---|---|
| would_block | recv 返回 EAGAIN/EWOULDBLOCK | 可以等待后续就绪 |
| eof | recv 返回 0 | 处理最后字节，再处理接收方向结束 |
| limit | 恰好用完本次预算 | 还有没有数据未知，需要安排续读，不能直接依赖新的 ET 通知 |

工作缓冲建议使用一个小的固定数组，不按整个 byte_limit 预分配。
每次 recv 请求长度不能超过剩余预算。只追加实际返回的正数字节；EINTR 重试。
byte_limit 为 0 无法取得进展，按契约拒绝。
如果刚好读满预算，即使对端已关闭，也返回 limit；下一次调用才能通过 recv==0 确认 EOF。

```bash
timeout 15s ./build/debug/epoll_exercises_test read_empty
timeout 15s ./build/debug/epoll_exercises_test read_limit
timeout 15s ./build/debug/epoll_exercises_test read_eof
timeout 15s ./build/debug/epoll_exercises_test et_drain
timeout 15s ./build/debug/epoll_exercises_test et_new_edge
timeout 15s ./build/debug/epoll_exercises_test et_budget_resume
```

et_drain 使用 8192 字节，不能只做一次小 recv。et_new_edge 验证读到 EAGAIN 后新输入再次通知。
et_budget_resume 限制首次预算为 2，随后不靠第二次事件而主动读剩余字节。
用户态 runnable 队列是之后完整事件循环的任务；这项检查只验证预算/续读机制，没有替你实现队列。
输入缓冲满时也应停止读，不能为满足“读到 EAGAIN”而突破内存上限。

## 6. D：非阻塞写、背压和半关闭（40 min）

实现 epoll_write_available(fd, bytes)：返回实际 sent 和是否因 EAGAIN 停止。
只按实际发送字节推进；空输入不做系统调用；Linux 使用 MSG_NOSIGNAL。
本函数不拥有 bytes，调用者保存未发后缀，在后续 EPOLLOUT 下继续。
此前已发送的前缀不能重发，未发的后缀不能丢弃。

```bash
timeout 15s ./build/debug/epoll_exercises_test write_empty
timeout 15s ./build/debug/epoll_exercises_test write_complete
timeout 15s ./build/debug/epoll_exercises_test write_backpressure
timeout 15s ./build/debug/epoll_exercises_test half_close
timeout 15s ./build/debug/epoll_exercises_test io_errors
```

write_backpressure 把本机发送缓冲缩小，发送 64 KiB，实际走到部分发送和 EAGAIN。
对端分批读取后，等待写就绪，继续未发后缀；全部完成时取消 OUT 关注。
检查字节数和内容，不检查固定的内核缓冲容量或固定吞吐。
half_close 要求在收到 RDHUP 后读完 request，recv==0 才确认 EOF，而且还能发送 reply。
RDHUP/HUP 本身不能替代 recv，也不表示应当立即丢弃连接的输入和输出。

空输出积累到非空时，应主动尝试一次写入或更新 OUT 关注，不能只等一个以前已经消费掉的 ET 可写事件。
完整输出状态机仍需要输出容量限制；本函数只处理你给出的一个 view。
系统错误抛异常时，读函数不承诺返回之前已读的前缀；调用方终止连接。
这不是自动恢复或 exactly-once 网络协议。

## 7. 全部检查与验收（20 min）

共 22 项：instance、empty_wait、validation、registry、ctl_failure、lt_partial、et_partial、
et_drain、et_new_edge、et_budget_resume、modify_interest、remove_interest、multiple_fds、batch_limit、
half_close、read_empty、read_limit、read_eof、write_empty、write_complete、write_backpressure、io_errors。

```bash
cmake --build --preset debug --target epoll_exercises_test --parallel 2 &&
timeout 15s ./build/debug/epoll_exercises_test all

cmake --preset asan &&
cmake --build --preset asan --target epoll_exercises_test --parallel 2 &&
timeout 15s ./build/asan/epoll_exercises_test all
```

完成标准：22 项通过，无 ASan/UBSan 报告，并能解释以下问题：

1. epoll 实例与被观察 socket 分别由谁关闭？为何 remove 不等于 close？
2. 为什么 wait 不直接返回网络字节？为何不能用事件数组容量当作返回事件数？
3. LT 下每次读一点仍能推进，而 ET 下为什么需要读到 EAGAIN 或主动续读？
4. 达到字节预算/输入上限时，如何既有界又不丢失处理机会？
5. 读半关闭后为何仍能回复？输出排空后为何取消 OUT？

这些检查不覆盖所有线程调度、多线程 epoll_wait、accept/connect、EPOLLONESHOT、严格 deadline、
fd 编号重用/重复 fd、所有 HUP/ERR 组合、真实大规模公平性或压力性能。
它们也没有完成 L11 parser/L06 queue 的业务集成。

## 8. 接回原来的 poll 练习（另安排 30–45 min）

先以 LT 替换 poll_once 的等待层，保留连接的输入/输出状态机。注册只做一次，关注变化用 MOD，退役先 DEL。
映射 IN/OUT/RDHUP/HUP/ERR 时依据连接契约，不直接把 epoll 的位数值当作 poll 的位数值。
处理读取、提交业务、排入回复之后，再判断是否回收连接。

LT 集成检查通过后再做 ET：增加 runnable 续读/续写任务，记录 budget 停止原因，限制单连接每轮工作量。
先对两个本地连接验证一个忙连接不妨碍另一个，再扩展 eventfd 唤醒、ONESHOT 与多线程。
这一完整集成目前没有预置入口，由你在 scratch 中完成；本节 22 项只验收上面的独立组件。
