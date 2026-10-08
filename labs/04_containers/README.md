# L04｜先会用 map/unordered_map，再理解冲突、迭代器与容器选择（约 2 小时）

本节从 L03 的 vector 出发，用标准容器完成三组观察。先学接口，再解释日志，最后查本机源码。现成程序已实现实验 A/B/C 的基线；你需要完成下面的变体、图和解释。

希望像 MiniVector 一样继续手写底层结构时，接着做[第 9 节](#9-手写底层结构额外约-46-小时)的 MiniHashMap 和 MiniBSTMap。前八节的观察约 2 小时；手写扩展另外安排约 4–6 小时，按实际进度拆开完成。

List、Heap、RingBuffer、Array、分块 Deque 和红黑树的脚手架也已提供，统一见[手写容器索引](SCAFFOLDS.md)。所有核心算法保留 TODO，每套配有阶段检查；这些额外练习不计入上述时间预算。

完成后应能回答：按 key 找 value 怎么写？为什么哈希冲突不会覆盖不同 key？为什么 unordered_map 扩桶会让 iterator 失效，却保留元素引用？有序查询会怎样影响容器选择？

## 1. 材料与运行入口（15 分钟）

第一次按本文顺序学习，遇到概念再打开材料。视频选看相关部分即可，全部观看另计时间。

| 材料 | 具体看什么 | 对应任务 |
|---|---|---|
| [L03 iterator 示例](../03_vector/iterators.cpp) | begin/end、`*it`、`++it`、erase 返回值；如果还不熟，先运行 vector_iterators | 实验 A |
| [R05：Standard Library Containers](https://www.youtube.com/watch?v=ZMUKa2kWtTk) | vector、map、unordered_map 的用途；list/deque 放到最后 | 实验 A、选型表 |
| [R06：Iterators in C++](https://www.youtube.com/watch?v=26aW6aBVpk0) | forward、bidirectional、random access；先跳过 ranges/views | 第 2 节、实验 C |
| [有序关联容器契约](https://eel.is/c++draft/associative.reqmts) | 搜 lower_bound、invalidate，核对有序查询和失效规则 | 实验 A/C |
| [无序关联容器契约](https://eel.is/c++draft/unord.req) | 搜 rehash、reserve、load_factor，核对桶与失效规则 | 实验 B/C |
| [项目材料目录](../../docs/RESOURCES.md)、[源码地图](../../docs/STL_SOURCE_MAP.md) | R05/R06 的材料说明；原源码地图记录 Mac libc++，Linux 使用本文第 6 节 | 源码阅读 |

工作草案持续更新，本实验只使用 C++20 接口。先读第 2 节，然后在项目根目录执行：

```sh
cmake --preset debug
cmake --build --preset debug --target container_observe
./build/debug/container_observe --interfaces
```

[observe.cpp](observe.cpp) 的函数与命令一一对应：

| 参数 | 阅读入口 | 已经实现的观察 |
|---|---|---|
| `--interfaces` | observe_interfaces() | 遍历、查找、重复 key、下标、范围查询、删除 |
| `--collisions` | observe_collisions() → observe_hash() | 默认/恒定 hash、桶分布、比较次数、增加桶数 |
| `--stability` | observe_stability() | rehash/reserve 后保留元素指针并重新取得 iterator；map 插入/删除 |
| 无参数或 `--all` | main() | 依次执行 A/B/C |

检查现有基线：

```sh
ctest --test-dir build/debug -R '^container_observe$' --output-on-failure
```

CTest 验证基线的特定语义；你的变体、选型表和图需要另外完成。程序没有性能计时，也没有访问失效 iterator。

## 2. 先认识本节的对象

key/value 就是“根据什么找／找到什么”。例如 `{20, "B"}` 中，20 是 key，字符串是 value。

```cpp
std::vector<std::pair<int, std::string>> sequence;
std::map<int, std::string> ordered;
std::unordered_map<int, std::string> hashed;
```

| 容器 | 元素顺序和 key | 按 key 查询 |
|---|---|---|
| `vector<pair<int, string>>` | 保留放入的位置；不会自动禁止重复 key | 本例用 std::find_if 逐个检查 pair.first |
| `map<int, string>` | 默认按 key 升序；每个 key 至多一个元素 | find(key)、lower_bound(key) |
| `unordered_map<int, string>` | 每个 key 至多一个元素；遍历顺序没有排序保证 | find(key) |

两个 map 的 iterator 解引用得到 `pair<const Key, T>`：`it->first` 是 key，`it->second` 是 value。不能通过 iterator 给 key 赋新值；非 const 容器的 value 可以修改。

基本查询写法：

```cpp
auto it = ordered.find(20);
if (it != ordered.end()) {
  std::cout << it->first << ':' << it->second << '\n';
}
```

find 找不到时返回 end，先判断再访问。unordered_map 的写法相同。vector 的 `v[20]` 表示下标 20，map 的 `m[20]` 表示 key 20，这两个含义不同。

| iterator 能力 | 容器 | 典型操作 |
|---|---|---|
| 随机访问 | vector、deque | ++it、--it、it + n、两个同容器 iterator 相减 |
| 双向 | map、list | ++it、--it；没有 it + n |
| 前向 | unordered_map | ++it；不保证支持 --it，没有 it + n |

操作仍要求位置合法：不能解引用 end，也不能对 begin 递减。`std::advance(it, n)` 可以帮你向前走，但在 map 上走 n 步仍需线性次递增。

## 3. 实验 A：先把接口用明白（25 分钟）

读 observe_interfaces()，先预测，再运行 `--interfaces`。输入依次为 `{30,"C"}, {10,"A"}, {20,"B"}`。

| 步骤 | 应当观察到什么 | 要解释的问题 |
|---|---|---|
| 遍历 | vector 为 30、10、20；map 为 10、20、30；unordered_map 不要求固定顺序 | 插入顺序与 key 顺序有什么区别？ |
| 查找 key 20 | 三者都得到 B | vector 的 find_if 为什么需要条件？ |
| 再追加/插入 `{10,"replacement"}` | vector 大小变成 4；两个 try_emplace 返回 inserted=false，原值还是 A | vector 为什么不自动去重？ |
| ordered.find(99) | 找不到，大小不变 | 查询是否一定会插入？ |
| ordered[99] | 插入 key 99，value 为空 string，大小增加 1 | 为什么不能用下标做纯查询？ |
| key 范围 [15,30) | 只打印 key 20 | 为什么结束位置用 lower_bound(30)？ |
| 删除 key 10 | erase(iterator) 返回指向 key 20 的 iterator | 为什么接住删除返回值？ |

try_emplace 返回两个结果：position 指向新插入或原有元素；inserted 表示是否新增。要替换已有 value，可以研究 insert_or_assign。下标在缺少 key 时插入默认值的行为见 [map.access](https://eel.is/c++draft/map.access)。

lower_bound(k) 找第一个 key **大于等于 k** 的位置；upper_bound(k) 找第一个 key **大于 k** 的位置。默认升序 map 中，`[lower_bound(15), lower_bound(30))` 对应 key 范围 [15,30)。边界查询也可能返回 end，不能直接假定可解引用。

你来做三个小改动，一次只改一个：

1. 查询 key 从 20 改为 25，补“找不到”的输出，避免解引用 end。
2. 在重复 key 步骤之后，用 insert_or_assign 更新 key 10，打印更新后的值与大小。
3. 范围改为 [10,20]，结束 iterator 使用 upper_bound(20)，预期输出 10、20。

每次修改后重新构建再运行。实验条件变化时同步调整相关 require，不要删除全部检查来获得 PASS。

**完成标准：**能独立写一次安全 find、遍历、更新和删除，并解释 `[key]` 与 `[index]` 的区别。

## 4. 实验 B：冲突、桶和负载因子（25 分钟）

先建立模型：

```text
key → hash(key) → 容器选择 bucket → 在其中判断 key 是否相等 → 得到 value
```

bucket 是元素分组的位置。hash 相同的 key 必须进入同一桶，不同 hash 也可能进入同一桶。**hash 相同不代表 key 相等**，不同 key 冲突后仍可同时存在。

| API | 含义 |
|---|---|
| size() | 元素总数 |
| bucket_count() | 桶总数，不是 vector 的 capacity |
| bucket_size(i) | 第 i 个桶中的元素数 |
| load_factor() | 平均每桶元素数：size / bucket_count |
| max_load_factor() | 容器使用的负载因子上限策略参数 |

```sh
./build/debug/container_observe --collisions
```

读 ConstantHash 和 CountingEqual：前者对所有 key 返回 0；后者在比较 key 时计数。两种表都插入 0–63，value 为 key * 10，再查询不存在的 key -1。日志字段含义：

| 字段 | 含义 |
|---|---|
| occupied | 非空桶数量 |
| largest_bucket | 最拥挤的桶中的元素数 |
| missing_key_equal_calls | 这一次失败查询的 key 相等判断次数，已清除其他阶段的计数 |

恒定 hash 应只有一个非空桶，里面放全部 64 个元素；增加桶数后仍聚在同一桶。默认 hash 的分布与计数记录实际结果，不写死桶数、遍历顺序或比较次数。

你来做：

1. 只改 observe_hash() 的 key_count，分别用 16、64、256；每次重新构建，记录两种 hasher 的四行日志。
2. 比较恒定 hash 的失败查询次数是否随 n 增长，解释它与“平均 O(1)、最坏 O(n)”的关系。
3. 保持 key 不变，只把 rehash 的桶数倍率从 4 改为 8；解释为什么加桶不能修好恒定 hash。

单次运行不能证明渐近复杂度。这里用计数说明退化机制，不宣布某容器快多少；平均/最坏复杂度和 rehash 复杂度分别查 [无序容器契约](https://eel.is/c++draft/unord.req)。

**完成标准：**画出同一桶中三个不同 key 的关系，说明平均负载低为什么仍可能有一个特别拥挤的桶。

## 5. 实验 C：iterator 与元素指针分开讨论（20 分钟）

```sh
./build/debug/container_observe --stability
```

逐行读 observe_stability()：先 find(10) 得到 iterator，再用 std::addressof(it->second) 保存 value 指针；请求比原桶数更大的桶数，确保发生 rehash。**旧 iterator 从这里开始不再使用**，重新 find，再比较新取得的 value 地址与保存的指针。

unordered_map rehash 调整桶的组织，标准保证元素指针和引用不失效；vector 重分配则会使原元素的指针、引用和 iterator 全部失效。依据：[无序容器要求](https://eel.is/c++draft/unord.req)、[vector.capacity](https://eel.is/c++draft/vector.capacity)。

### reserve 与 rehash 的参数

- rehash(b)：请求至少 b 个桶，还需满足当前元素数与负载因子的要求；实际桶数可更大。
- reserve(n)：为目标**总元素数** n 准备桶数，按当前 max_load_factor 计算；不会创建 n 个元素，也不是“再多留 n 个元素”。

程序设置 max_load_factor(0.5F)，再 reserve(100)，需要为 100 个总元素准备至少 200 个桶，实际结果由实现选择。size 不变，也不会构造 100 个 string。reserve 的契约与 vector 的“不增长时直接无操作”不同，调用后应重新取得 iterator。定义见 [rehash/reserve 要求](https://eel.is/c++draft/unord.req)。

### 本实验用到的失效规则

| 操作 | 已有元素 iterator | 已有元素指针/引用 |
|---|---|---|
| vector 重分配 | 全部失效 | 全部失效 |
| map 插入 | 保持有效 | 保持有效 |
| map 删除一个元素 | 只使被删元素的失效 | 只使被删元素的失效 |
| unordered_map 发生 rehash | 全部失效 | 保持有效 |
| unordered_map 插入 | 可能失效；发生 rehash 时失效 | 保持有效 |i
| unordered_map 删除一个元素 | 只使被删元素的失效 | 只使被删元素的失效 |
| 这三个容器 clear | 元素 iterator 全部失效 | 元素指针/引用全部失效 |

表只讨论元素句柄，end 的特殊规则单独查契约。map 的规则见 [有序关联容器要求](https://eel.is/c++draft/associative.reqmts)，unordered_map 的规则见 [无序关联容器要求](https://eel.is/c++draft/unord.req)。删除元素或销毁容器后，地址数值没变也不能继续使用旧引用或指针。

你来做：

1. reserve(100) 前后额外打印 load_factor，解释为什么 size 没变，负载因子却可能变化。
2. 保留 key 20 的 value 指针，删除 key 10 后安全访问 key 20；不要访问被删除元素的指针。
3. 分别画 vector 扩容、unordered_map rehash 的前后图，标出“元素位置”和“遍历路径”怎样变化。

**完成标准：**能解释旧 iterator 不可用而旧 value 指针仍可用，不拿失效 iterator 做“试试看还能不能读”的实验。

## 6. 对照当前机器读源码（20 分钟，到时停止）

当前构建使用 GCC 13.3，对应 Linux 的 libstdc++。先确认库，而不只看编译器名；Clang 也可以使用 libstdc++：

```sh
c++ -dM -E -x c++ -include vector /dev/null | rg '__GLIBCXX__|_LIBCPP_VERSION'
```

`__GLIBCXX__` 对应 libstdc++，`_LIBCPP_VERSION` 对应 libc++。构建时指定其他编译器，命令中的 c++ 也要换成它。下面路径已在当前 GCC 13 环境核对，其他版本按实际头文件路径调整：

```sh
LAB_STDLIB_DIR=/usr/include/c++/13
rg -n -m 4 '_M_buckets|_M_before_begin|_M_rehash' "$LAB_STDLIB_DIR/bits/hashtable.h"
rg -n -m 4 'struct _Hash_node|_M_nxt' "$LAB_STDLIB_DIR/bits/hashtable_policy.h"
rg -n -m 4 '_Rb_tree_node_base|_M_parent|_M_left|_M_right' "$LAB_STDLIB_DIR/bits/stl_tree.h"
rg -n '_M_lower_bound\(_Link_type' "$LAB_STDLIB_DIR/bits/stl_tree.h"
```

在编辑器打开命中位置附近，只完成两件事：

1. **哈希表，10 分钟。** 找 `_Hash_node` 的 next 链接、`_M_buckets`、`_M_rehash`，画“桶数组／节点／节点中的 key-value”。GCC 的桶入口可能指向该桶首节点的前驱，节点还连接在整体链表中；先读 hashtable.h 顶部设计注释，再按本机布局画，不直接套“每桶独立链表”。回答 rehash 改动了什么、为什么能保留元素地址。
2. **树，10 分钟。** 找 `_Rb_tree_node_base` 的 parent/left/right，读 `_M_lower_bound` 的比较和向左/右走分支。用 10、20、30 画说明查找路径的概念树，解释默认中序遍历为什么升序。图不要求复现本次插入后的真实树形；先不追旋转和完整平衡算法。

如果回到 Mac/libc++，用[原源码地图](../../docs/STL_SOURCE_MAP.md)里的 `__hash_table`、`__tree`，不在 Linux 上找这些名字。字段、桶映射算法和具体树形是实现细节；公开语义与复杂度由标准契约约束。

## 7. 把观察变成容器选择（15 分钟）

先把单次 key 比较/hash 的开销视为常数。n 是元素数，k 是输出元素数。

| 操作 | 未排序 vector | 已按 key 排序的 vector | map | unordered_map |
|---|---|---|---|---|
| 按 key 查询 | O(n) | 二分 O(log n) | O(log n) | 平均 O(1)，最坏 O(n) |
| 保持组织并新增 key | 尾部追加摊还 O(1)，不自动去重 | 一般 O(n)，要移动后缀 | 一般 O(log n) | 平均 O(1)，最坏 O(n) |
| 按 key 范围输出 | 扫描 O(n)，结果未必有序 | O(log n + k) | O(log n + k) | 扫描 O(n)，结果未必有序 |

排序 vector 的二分要求输入已排序；map 用成员 lower_bound。对 map 的双向 iterator 使用通用 std::lower_bound，比较次数虽可为 O(log n)，iterator 移动次数仍可为 O(n)。参考 [二分查找算法要求](https://eel.is/c++draft/alg.binary.search)。本节学习接口与机制，不是运行时间排行榜。

填自己的选择表，可以给多个候选，但必须写约束和反例：

| 场景 | 你的候选容器 | 为什么 | 哪个条件变化后会换容器 |
|---|---|---|---|
| 大量连续扫描，偶尔末尾追加 | 待填 | 待填 | 待填 |
| 根据订单 ID 查订单，没有排序需求 | 待填 | 待填 | 待填 |
| 查询价格区间 [low, high] | 待填 | 待填 | 待填 |
| 经常在已找到的位置中间插入/删除 | 待填 | 待填 | 待填 |
| 对外保留元素引用，还要继续新增 | 待填 | 待填 | 待填 |

补充：list 在已知位置插入/删除可为 O(1)，但先走到位置可能要 O(n)，节点分配和指针跳转也有成本。deque 支持随机访问和两端插入，标准不要求整体连续，也不规定固定块大小；两端插入可保持元素引用，iterator 规则另外核对。[list 契约](https://eel.is/c++draft/list.overview)、[deque 概览](https://eel.is/c++draft/deque.overview)、[deque 插入](https://eel.is/c++draft/deque.modifiers)。

讨论实际性能时，再考虑元素大小、访问模式、分配、局部性、hash/比较成本和偶发 rehash 延迟。相同复杂度不代表耗时相同；计时留到 Release 下另做，不给 CTest 添加速度门槛。

## 8. 交付与完成标准

使用[实验记录模板](../../docs/EXPERIMENT_TEMPLATE.md)，记录在自己选择的笔记文件中：

1. A 的预测、实际输出，以及三个接口变体的结果。
2. B 在 16、64、256 个 key 下的桶分布和失败查询计数，解释为什么加桶不能修复恒定 hash。
3. C 的两张前后图、失效解释，以及删除另一个元素的验证。
4. 本机哈希节点/桶图和树查找路径图，标明标准保证与本机实现。
5. 五行选型表，每行包含一个反例或约束变化。

能闭卷回答这四个问题，就可以继续 L05：

- map.find(20) 找不到时返回什么？map[20] 又会做什么？
- hash 冲突后，为什么两个不同 key 都能查到？
- unordered_map rehash 后哪些句柄可以保留？删除元素后呢？
- 有序范围查询为什么会改变你对 map、unordered_map、排序 vector 的选择？

如果 A 的接口还不熟，先只完成 A 和变体；B/C 与源码阅读下次继续。“基线 PASS”不代表已经完成选型和源码解释。

## 9. 手写底层结构（额外约 4–6 小时）

目前 MiniVector 已练了连续存储。这里补两个使用独立节点的容器：先 MiniHashMap，再 MiniBSTMap。源码中的 TODO 由你填写；检查和初始状态已经提供。暂不实现完整 STL 接口、泛型 key/value、迭代器、拷贝/移动及树的自动平衡。

| 文件 | 用途 |
|---|---|
| [mini_hash_map.hpp](mini_hash_map.hpp) | 哈希表脚手架；在这里实现桶、链、查询和 rehash |
| [mini_hash_map_test.cpp](mini_hash_map_test.cpp) | 哈希表的 8 组检查 |
| [mini_bst_map.hpp](mini_bst_map.hpp) | 普通 BST 脚手架；在这里实现树查找、遍历和删除 |
| [mini_bst_map_test.cpp](mini_bst_map_test.cpp) | BST 的 8 组检查 |
| [check_runner.hpp](check_runner.hpp) | 通用检查运行器；不包含容器算法 |

两个容器固定为 int key、string value。节点结构和状态字段已经列出，std::vector 可以用作桶数组或遍历结果，元素本身由你的节点存储。不要用 std::map/unordered_map 代替待实现的存储；它们只在检查代码里作为结果对照。

### 9.1 构建与运行：先看到失败，再逐项实现

```sh
cmake --preset debug
cmake --build --preset debug --target mini_hash_map_test mini_bst_map_test
./build/debug/mini_hash_map_test empty
./build/debug/mini_bst_map_test empty
```

empty 只检查已经提供的初始状态，应该通过。真正的操作仍是 TODO，会报告失败：

```sh
./build/debug/mini_hash_map_test insert_find
./build/debug/mini_bst_map_test insert_find
```

可以指定表里的任一 case 名，也可以不传参数跑全部。出现 `TODO: ...` 是待实现提示，不能通过删除 require 或硬编码检查输入来完成任务。每次改头文件后都重新构建。两个练习目标暂不加入 CTest，现有 container_observe 通过不代表手写容器通过。

### 9.2 MiniHashMap：从桶和节点链开始

本练习采用“每桶直接指向首节点”的简化结构，与你刚读的 GCC 桶入口指向前驱的布局不同：

```text
buckets[0] → [key/value | next] → [key/value | next] → nullptr
buckets[1] → nullptr
buckets[2] → [key/value | next] → nullptr
```

先画这张责任图：桶数组持有所有链的入口，容器负责释放每个节点；释放一个节点前仍需保留继续访问剩余链的方式。只释放桶数组不会释放节点。

接口契约：

| 接口 | 本练习要求 |
|---|---|
| insert(key, value) | 新增返回 true；重复 key 返回 false，原 value 不变 |
| find(key)，含 const 重载 | 找到返回 value 指针，找不到返回 nullptr；不插入 |
| erase(key) | 删除并释放恰好一个节点；存在返回 true，否则 false |
| reserve(n) | 为目标总元素数 n 准备桶，不创建元素，不缩小现有桶数 |
| rehash(b) | 桶数至少为 max(b, size, 1)；可自行选择更多桶 |
| clear() | 释放全部节点，清空桶入口，size 归零，保留桶数组大小 |
| bucket_size(i) | 统计该桶节点数；越界抛 std::out_of_range |

最大负载因子固定为 1，每次成功新增后 size <= bucket_count。Hash 要求可通过 const 对象调用且不抛异常，脚手架已经做静态检查。增长、reserve、rehash 都保留已有元素地址；erase 只使被删除元素的指针失效。rehash 新桶分配失败时，旧状态必须保持不变。

实现顺序和阶段检查：

| 顺序 | 实现任务 | 运行 case |
|---|---|---|
| 1 | 先设计 clear 释放节点；写 find 和不需扩桶时的 insert | insert_find |
| 2 | 写 rehash/reserve 和自动增长，补 bucket_size；避免把旧链丢失或连成环 | reserve_rehash、growth |
| 3 | 写 erase，考虑删除首节点、链中间节点和不存在的 key | erase、collisions |
| 4 | 反复清空、复用，最后运行完整对照 | clear_reuse、random |

可使用 hash(key) 与当前桶数计算桶索引；负数 key 也要支持。这里不指定增长倍率或素数策略，检查只验证容量约束和内容。不要直接把 int key 当桶下标。

rehash 前先回答：新桶数组什么时候可以提交？旧节点的 next 要改变，元素本身是否必须复制？遇到异常时，哪些资源还归旧容器所有？先画图，再实现。

### 9.3 MiniBSTMap：普通二叉搜索树

树的不变量是：左子树所有 key 小于当前 key，右子树所有 key 大于当前 key，每个 key 唯一。

```text
       20
      /  \
    10    30
   /  \
  5   15
```

每个节点保存 key/value、left 和 right；root_ 是根入口。容器负责释放所有节点。父指针暂不要求，普通 BST 不需要红黑树的颜色字段或旋转修复。

| 接口 | 本练习要求 |
|---|---|
| insert(key, value) | 新增返回 true；重复 key 返回 false，原 value 不变 |
| find(key)，含 const 重载 | 找到返回 value 指针，找不到返回 nullptr |
| lower_bound_key(key) | 返回最小的 >= key 的 key；不存在返回 std::nullopt |
| items() | 返回按 key 升序排列的 vector 快照，快照修改不影响树 |
| erase(key) | 删除对应节点，维护搜索树不变量和 size；存在返回 true |
| clear() | 释放整棵树，root 置空，size 归零；重复调用安全 |

插入不使已有元素指针失效。为减少第一版删除算法的约束，本练习约定每次 erase 后都重新获取指针；这里没有承诺标准 map 那样的删除稳定性。所有复制/移动操作暂时禁用，避免浅拷贝节点指针后重复释放。

实现顺序和阶段检查：

| 顺序 | 实现任务 | 运行 case |
|---|---|---|
| 1 | 先设计 clear，再写 insert、两个 find | insert_find |
| 2 | 写 lower_bound_key，处理空树、精确命中及两个边界 | lower_bound |
| 3 | 写中序遍历，返回独立快照 | traversal |
| 4 | 删除叶节点、单孩子节点、根节点和最后一个节点 | erase_simple |
| 5 | 删除双孩子节点；检查选中的前驱/后继自身还有孩子时的连接 | erase_two_children |
| 6 | 验证顺序插入、清空复用，最后跑完整对照 | skew_clear_reuse、random |

删除前分别画三种孩子数量的图，并标出谁将接替“指向被删除节点的链接”。修改根节点也属于要处理的链接变化。不要只移除目标 value 而丢失其子树。

普通 BST 的查询/插入/删除是 O(h)，h 为树高。升序插入可能退化成链，最坏 O(n)；它仍应给出正确结果。完成后再解释平衡树为什么能控制高度，完整红黑树留作后续专项。

### 9.4 最终验收

两套实现完成后：

```sh
cmake --build --preset debug --target mini_hash_map_test mini_bst_map_test
./build/debug/mini_hash_map_test
./build/debug/mini_bst_map_test
cmake --preset asan
cmake --build --preset asan --target mini_hash_map_test mini_bst_map_test
./build/asan/mini_hash_map_test
./build/asan/mini_bst_map_test
```

目标是每套 8 组通过，并在可工作的 ASan/UBSan 环境中无泄漏、重复释放和非法访问报告。检查并不证明所有输入都正确；随机对照固定 seed 为 404/405，未固定桶数量或 BST 树形。检查不包含分配失败注入，rehash 的异常保证还需你根据提交顺序解释。

最终对比三种手写容器：MiniVector 扩容搬迁元素，MiniHashMap 扩桶重连节点，MiniBSTMap 插入改变树链接。分别解释所有权、地址稳定性、平均/最坏复杂度，以及为什么当前 BST 还不能提供标准 map 的复杂度保证。
