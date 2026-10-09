# L09｜局部性、访问依赖与 AoS/SoA（主线 4h）

先完成 L08。你要自己写访问路径，并解释结果相同而访存路径不同。正确性用 Debug 检查，计时用 Release。
主线 4h 包含阅读与基础实现；大工作集扫描和硬件性能计数器另计。

## 1. 阅读顺序（40min）

从 [R10](../../docs/RESOURCES.md) 的 CMU Memory Hierarchy、Cache Memories 开始，重点看 spatial/temporal locality、cache line、working set。
补充原作者 [What Every Programmer Should Know About Memory](https://www.akkadia.org/drepper/cpumemory.pdf)：§3.3.2 Measurements of Cache Effects 与 §6.2.1 Optimizing Level 1 Data Cache Access 的字段布局例子。
这是机制参考，不把文中某款机器的参数或倍数直接应用到当前服务器。

带着三个问题读：同样求和为什么快慢不同？随机访问为什么多一份下标？只读取 price 时，AoS 的 quantity/timestamp 是否可能随 cache line 带入？

## 2. 入口与文件（20min）

| 文件 | 用途 |
|---|---|
| [observe.cpp](observe.cpp) | 已完成标准算法基线；3 轮交替计时和 checksum |
| [locality_exercises.hpp](locality_exercises.hpp) | 五个 TODO：下标访问、stride、依赖访问、两种布局 |
| [locality_exercises_test.cpp](locality_exercises_test.cpp) | 边界、64-bit 累加、固定输入/种子检查 |
| [benchmark.cpp](benchmark.cpp) | 已完成测量脚手架；直接调用你补全的五个函数 |

```bash
cmake --preset debug &&
cmake --build --preset debug --target locality_test --parallel 2 &&
./build/debug/locality_test --help
cmake --preset release &&
cmake --build --preset release --target locality_observe --parallel 2 &&
./build/release/locality_observe
```

case：ordered、stride、chase、layout、bounds；无参或 all 运行全部。初始 TODO 失败是正常状态，不删除检查来宣称完成。

## 3. 看懂基线（40min）

基线有 32768 个正整数，约 128 KiB；AoS 有三个 uint64_t 字段，约 768 KiB。
另有 price 列和 permutation 下标，分别打印占用字节数。随机种子 42 固定，但具体排列不要求跨标准库完全相同。

| 标签 | 访问路径 | checksum |
|---|---|---|
| sequential | 连续读取全部整数 | 1 到 count 的和 |
| permutation | 读下标后读对应整数 | 与顺序相同 |
| AoS-price | 按记录访问，只计算 price | 与整数基线相同 |
| SoA-price | 连续读 price 列 | 与 AoS 相同 |

每个路径先 warmup，再交替次序测 3 轮；打印样本和中位数，没有速度阈值。默认规模用于展示方法，不保证足以区分某种硬件效应。
observer 使用标准算法，没有给练习函数实现；不要把 accumulate/transform_reduce 填入 TODO 来跳过自己写循环。

permutation 多读下标数组，不是只有随机/连续一个差别。AoS 与 SoA 字节数不同，比较的是读取一个字段的场景，不代表所有业务都应使用 SoA。
算法展开/向量化、工作集、调度与计时精度都可能影响结果；没有直接统计 cache/TLB miss。

## 4. 自己实现（85min）

1. sum_ordered：推演 order={2,0,3,1}。重复下标表示重复访问，空 order 不访问数据，越界抛 out_of_range。通过 ordered。
2. sum_stride：5 个元素、stride=2 访问 0/2/4；stride=0 抛 invalid_argument。想清 SIZE_MAX 如何停止，避免无符号回绕。通过 stride。
3. chase：next={2,3,1,0}，画 0→2→1→3→0。返回访问节点下标的和，steps=4 与 8 不同；下一地址依赖本次读取。最后获得的后继不再访问，steps=0 不读 start。通过 chase。
4. sum_prices_aos/soa：只累加 price，quantity/timestamp 故意取其他值。使用 uint64_t 累加，不能先在 uint32_t 求和再转型。通过 layout。
5. 统一错误路径，通过 bounds 和 all。

```bash
./build/debug/locality_test ordered
./build/debug/locality_test chase
./build/debug/locality_test all
```

## 5. 直接测量自己的实现（35min）

五个函数完成、正确性检查通过后，不必另写计时入口；已有 runner 会调用你的 header：

```bash
cmake --preset release &&
cmake --build --preset release --target locality_benchmark --parallel 2 &&
./build/release/locality_benchmark
./build/release/locality_benchmark --count 65536 --stride 16
./build/release/locality_benchmark --help
```

默认 count=32768、stride=8；两参数均接受 1..262144，顺序任意，每个只能出现一次。
不接受零、负数、超限、缺值或未知选项；错误参数返回 2。TODO 或错误 checksum 返回 1，成功为 0。
默认总数据约 2 MiB，上限约 15 MiB（取决于 sizeof）；固定 warmup 一轮、计时三轮交替次序，没有长时间循环。

runner 提供数据生成、计时和校验，访问循环仍完全来自你的实现。分配/排列生成不算进遍历时间。
sequential/permutation 同样通过 sum_ordered 读取下标，比较路径更接近；与 observer 直接连续累加的基线不能视为完全相同工作量。
chase 的 next 由完整排列连成单环，访问 count 次，不会偶然只绕一个短环。

| 路径 | 访问次数 | 期望 checksum |
|---|---|---|
| 两种下标顺序、AoS、SoA | count | 正整数值的和 count×(count+1)/2 |
| stride | m=1+(count−1)/stride | m+stride×m×(m−1)/2，不能与完整遍历直接比总耗时 |
| chase | count | 节点下标之和 count×(count−1)/2，按接口契约不累加 values |

每轮输出次数与 checksum，防止把不同工作量或不同结果直接比较；没有性能阈值。
默认用小规模，扩展前协调共享机器资源。打印 checksum 避免无用计算，不用 volatile 代替测量设计。
Sanitizer/Debug 用于排错；Release 仍包含你接口的边界校验，其成本也是测量的一部分。

## 6. 验收（20min）

五项检查通过，记录字节数、下标字节数、次数、checksum、编译模式与每轮样本。
解释预先已知的 permutation 下标与 chasing 地址依赖，画三条记录的 AoS 与三列 SoA。
结合列式查询区分参与计算的字段与可能进入 cache 的字节。列两个未排除因素，不把时间直接解释为硬件 miss 数。完成后进入 L10。
