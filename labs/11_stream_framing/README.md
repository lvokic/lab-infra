# L11｜字节流、长度前缀与部分 I/O（5h）

本节从“收到了字节”推进到“收到了完整消息”。先独立完成 parser，再接 socket；不要把一次 `recv` 当成一条消息。已有的 `observe.cpp` 保留为入门观察，新增两个可编译的练习目标，协议和发送循环由你实现，核心函数全部是 TODO。

## 1. 材料与阅读顺序（30 min）

| 材料 | 只读这些部分 | 读后回答 |
|---|---|---|
| [recv(2)](https://man7.org/linux/man-pages/man2/recv.2.html) | DESCRIPTION、RETURN VALUE、EINTR/EAGAIN | 为什么 `recv(fd, buffer, 100, 0)` 可能只返回 3？返回 0 是什么？ |
| [send(2)](https://man7.org/linux/man-pages/man2/send.2.html) | RETURN VALUE、ERRORS、MSG_NOSIGNAL | 成功返回说明什么？发送 100 字节只返回 20 后下一次从哪里开始？ |
| [TCP(7)](https://man7.org/linux/man-pages/man7/tcp.7.html) | DESCRIPTION 中 byte stream 与 record boundaries | 哪一层定义消息边界？ |
| [bind(2)](https://man7.org/linux/man-pages/man2/bind.2.html)、[getsockname(2)](https://man7.org/linux/man-pages/man2/getsockname.2.html) | SYNOPSIS、DESCRIPTION | 为什么本实验绑定 `127.0.0.1:0`，而不是固定端口？ |
| [shutdown(2)](https://man7.org/linux/man-pages/man2/shutdown.2.html) | SHUT_WR 与 SHUT_RDWR | 停止写入后还能读取回复吗？ |

先读前三项完成 A/B，再查后两项做 D。系统调用和协议第一次接触时可以增加预算；不用先读完整网络教材。

## 2. 文件、目标与运行入口

| 文件 | 你的任务 | 对应目标 |
|---|---|---|
| `observe.cpp` | 运行，解释输出，再画字节序列 | `stream_io`（已有观察） |
| `frame_decoder.hpp` | 实现增量解析、长度限制、EOF 检查 | `framing_test` |
| `frame_decoder_test.cpp` | 阅读输入和断言；保留检查不修改成迁就错误实现 | 上述目标的检查代码 |
| `blocking_io.hpp` | 实现 `stream_lab::send_all`；fd RAII 已提供 | `stream_io_test` |
| `stream_io_test.cpp` | 真实 socketpair、TCP loopback 及错误路径检查 | 上述目标的检查代码 |

在仓库根目录运行：

```bash
cmake --preset debug &&
cmake --build --preset debug --target stream_io framing_test stream_io_test --parallel 2 &&
./build/debug/stream_io &&
./build/debug/framing_test --help &&
./build/debug/stream_io_test --help
```

只检查当前实现的部分：

```bash
timeout 10s ./build/debug/framing_test complete
timeout 10s ./build/debug/framing_test every_split
timeout 10s ./build/debug/stream_io_test socketpair
```

全部检查：

```bash
timeout 10s ./build/debug/framing_test all
timeout 10s ./build/debug/stream_io_test all
```

`all` 也可以省略；成功退出 0、检查失败 1、未知参数 2。未实现时出现 `TODO` 失败是预期结果，练习目标不加入默认 CTest。`timeout` 用于截住错误实现造成的等待；它不是程序的协议或同步手段。

## 3. A：先解释观察程序（30 min）

观察程序通过 `AF_UNIX/SOCK_STREAM` 在一个进程内创建两个端点，先发送 2 字节，再发送剩余 7 字节。读取每次最多 3 字节。

发送的完整字节序列是：

```text
00 00 00 05  51 55 41 4e 54
└──长度 5──┘ └── QUANT ──┘
```

你通常会看到三次 `recv bytes=3`，最终 `Decoded payload=QUANT`。这里读取长度上限是人为设定的，不表示接收方看到了三条消息。发送调用的 2/7 边界也没有保留在读取调用里。

完成三个问题：

1. 将读取数组改为 1、2、8 字节，记录每次返回的字节数和拼接结果。
2. 解释为什么必须拼接收到的字节，而不能只解析第一次读取。
3. 找到 `shutdown(SHUT_WR)`，说明为何先读完排队的数据，随后才能读到 0。

现有程序先收齐所有字节才解析，所以还不是增量 parser，也不能推广为一直在线的消息服务。这正是下一阶段要实现的部分。

## 4. B：实现独立 FrameDecoder（2h）

固定协议：**4 字节 unsigned big-endian 长度 + payload**。长度不包括 header；允许长度 0；单帧 payload 上限 65536 字节。payload 是任意二进制内容，可以包含 `\0` 和 `0xff`，不能调用 `strlen` 判断长度。

接口已经在 `frame_decoder.hpp` 给出：

| 接口/输入 | 必须产生的行为 |
|---|---|
| `feed(bytes)` | 依次消费本次输入；返回本次完成的所有 payload |
| 空 `feed` | 不生成帧，不丢失此前状态 |
| 只有部分 header/body | 保存不完整状态，不提前生成消息 |
| 一次包含多帧 | 返回全部完整帧，保持顺序；残留半帧留到下一次 |
| 空帧 | 返回一个空字符串，与“没有完成帧”的空 vector 区分 |
| 长度恰为 65536 | 接受 |
| 长度超过 65536 | 四字节 header 收齐时立即抛 `std::length_error`，不得先分配 body |
| `finish()` 在帧边界 | 成功，标记输入结束；重复 `finish` 成功 |
| `finish()` 遇到半个 header/body | 抛 `std::runtime_error`，进入 failed 状态 |
| 超限/截断进入 failed 后 | `feed` 与 `finish` 抛 `std::logic_error`，不再继续解析 |
| 成功 `finish` 后 `feed` | 抛 `std::logic_error`，包括空输入 |

若同一次 `feed` 前半段有完整帧、后半段超限，整个调用抛异常；本接口不保证异常前的完整帧还能取回。协议错误后关闭该连接，不尝试从任意字节重新寻找 header。内存分配失败不纳入本阶段的恢复保证。

先画出两个状态：“正在收 header”“正在收 body”，在图上标明：已有多少字节、目标长度、完成后转移到哪里。脚手架提供的字段只是帮助你表达这些状态，不要求照某种固定代码写。

建议实现顺序：

1. 完整帧与空输入，通过 `empty_input`、`complete`。
2. 两段输入与逐字节输入，通过 `every_split`、`bytewise`。
3. 多帧与二进制 payload，通过 `multiple_and_empty`、`binary`。
4. 长度上限与永久失败，通过 `limit`、`oversize`。
5. EOF 与已结束输入，通过 `truncated_header`、`truncated_body`、`finished`。
6. 最后运行 `random_chunks`：固定 seed 的 30 轮分片，结果必须相同。

检查清单共 12 项，名称与上述步骤一致。`every_split` 遍历一帧所有“两段切分”位置；`random_chunks` 补充多帧、多段组合，但它们并不穷举任意协议序列。

内存思考：decoder 的未完成帧应有上界；返回的 `vector<string>` 可能包含很多已完成帧，整个服务仍需限制每次读取量和业务队列。单帧上限不等于整个进程的内存上限。

## 5. C：完成阻塞发送循环（45 min）

在 `blocking_io.hpp` 中只实现 `send_all`。`UniqueFd` 已经完成，用来保证测试结束或抛异常时关闭 fd。

| `send` 返回 | 你需要做什么 |
|---|---|
| 正数 | 只推进实际发送的字节数；仍有剩余就继续 |
| `-1` 且 `errno == EINTR` | 保留 offset，重试 |
| `-1` 且其他错误 | 抛 `std::system_error`，保留 errno 说明 |
| 正长度请求返回 0 | 抛 `std::runtime_error`，避免无限循环 |
| 输入为空 | 直接成功，不访问 fd |

本函数只用于阻塞 fd。不能拿它处理 nonblocking 的 EAGAIN；L12 会把“剩余 offset”变成跨事件调用保存的状态。Linux 上发送时使用 `MSG_NOSIGNAL`，这样已断开的连接表现为可处理的系统调用错误，而不会由 SIGPIPE 直接终止进程。

运行顺序：`empty` → `socketpair` → `invalid_fd` → `closed_peer`。小数据真实传输、空输入和断开错误均有检查；测试没有强制制造 EINTR，也没有保证阻塞 `send` 一定返回部分成功。该分支还要通过代码审阅，或在选做中用可注入的系统调用适配器检查。

## 6. D：真实 TCP loopback 与组合练习（45 min）

先阅读 `stream_io_test.cpp` 的 `tcp_loopback` 检查：它已经提供连接建立的完整测试支撑，学习重点是理解每个系统调用，不要求先记住 `sockaddr_in` 的所有细节。

1. 服务端 `socket`、绑定 `INADDR_LOOPBACK`，端口设为 0。
2. `listen` 后用 `getsockname` 得到操作系统分配的端口。
3. 客户端连接该地址，服务端 `accept`，两个已连接端点使用同一个 `send_all`。
4. 发送小段二进制数据、`shutdown(SHUT_WR)`，接收方每次最多读取 3 字节，读到 EOF 后比较字节序列。
5. 运行 `stream_io_test tcp_loopback`，确认协议不依赖 AF_UNIX。

然后自己在 `scratch/` 写组合练习：发送三个长度前缀消息；接收循环每收到一次 bytes 就调用 `FrameDecoder::feed`，对输出消息逐条记录；`recv == 0` 时调用 `finish`。分别选择空消息、包含零字节的消息和普通消息，检查内容和顺序。

**组合练习目前没有独立自动检查**：`framing_test` 检查解析器，`stream_io_test` 检查发送与真实传输，两者各自独立。不要把这两个目标通过描述成完整协议服务已验收。

## 7. 完成标准与下一步（30 min）

交付：12 项 parser 检查、5 项阻塞 I/O 检查全部通过；一张增量状态图；组合练习的消息序列；说明发送成功不等于应用层已经处理消息。

用 ASAN/UBSAN 检查边界：

```bash
cmake --preset asan &&
cmake --build --preset asan --target framing_test stream_io_test --parallel 2 &&
timeout 10s ./build/asan/framing_test all &&
timeout 10s ./build/asan/stream_io_test all
```

不以 sleep 或强制网络分包来验收 parser；直接切分输入才能稳定验证每个状态。测试只使用本机 socketpair 和 `127.0.0.1:0`，不监听公网，不占固定端口。

下一节保留这套协议，先把 I/O 层变成非阻塞事件状态机，再接 parser。TLS、连接超时、信号注入和完整重连策略属于后续扩展，本节未自动覆盖。
