# L11｜部分 I/O、长度前缀与 TCP loopback（5h）

材料：R11/R12。预置 stream_io 用 AF_UNIX/SOCK_STREAM socketpair 在本机观察字节流，避免外部网络依赖。第二阶段由你添加 TCP loopback。

## 固定协议
4 字节 big-endian length + payload；允许空消息；单帧上限 64 KiB。长度超限立即拒绝，在分配前检查。EOF 时若仍有半个 header/body，报告不完整消息。

## 步骤
1. 预测 observe.cpp：两次 send 和每次最多 3 字节 recv 为什么不是一一对应。
2. 写独立 feed(bytes) 增量 decoder；保留不完整片段，可在一次 feed 中输出多条完整消息。
3. 测试每一个分片位置、header 被逐字节拆开、空帧、多帧、oversize、EOF 截断；加入固定 seed 的随机切分。
4. 实现 send_all；区分部分返回、EINTR、错误/关闭。在 nonblocking 版本把 EAGAIN 留给 L12 状态机处理。
5. TCP loopback 服务端绑定 127.0.0.1:0，由 getsockname 获取端口；配合 thread/accept/connect 传输同一协议。
6. 不用 sleep 强制制造所谓“粘包”；直接控制 decoder 输入切分更确定。socket 的实际返回分块记录为观察。

验收：独立 parser 不依赖某种 recv 长度；任意分片保持消息内容/顺序；EOF 与超限有明确定义；fd 通过 RAII 关闭。

追问：大帧慢速传输如何防止内存耗尽？Linux MSG_NOSIGNAL 与 macOS SO_NOSIGPIPE 对关闭连接写入有什么关系？
