# 手写容器脚手架与检查索引

这里提供接口、状态布局、资源管理接线和检查代码；核心算法保留 TODO。每次选一套完成。最初仅 empty 检查通过，运行其他 case 会报告 TODO；这不代表算法已经实现。

## 1. 已提供哪些练习

| 练习 | 核心文件 | 检查目标 | 下一组检查 |
|---|---|---|---|
| 连续动态数组 MiniVector | [L03/mini_vector.cpp](../03_vector/mini_vector.cpp) | mini_vector | 沿用原 main 的检查 |
| 分离链接哈希表 MiniHashMap | [mini_hash_map.hpp](mini_hash_map.hpp) | mini_hash_map_test | insert_find |
| 普通搜索树 MiniBSTMap | [mini_bst_map.hpp](mini_bst_map.hpp) | mini_bst_map_test | insert_find |
| 双向链表 MiniList | [mini_list.hpp](mini_list.hpp) | mini_list_test | ends |
| 二叉堆 MiniHeap | [mini_heap.hpp](mini_heap.hpp) | mini_heap_test | push_pop |
| 环形缓冲区 RingBuffer | [ring_buffer.hpp](ring_buffer.hpp) | ring_buffer_test | fifo |
| 固定数组 MiniArray | [mini_array.hpp](mini_array.hpp) | mini_array_test | access |
| 分块双端队列 MiniDeque | [mini_deque.hpp](mini_deque.hpp) | mini_deque_test | ends |
| 红黑树 MiniRBTreeMap | [mini_rb_tree_map.hpp](mini_rb_tree_map.hpp) | mini_rb_tree_map_test | insert_find |

L04 每套有 8 组检查，检查源文件名为“目标名.cpp”，例如 [mini_list_test.cpp](mini_list_test.cpp)。MiniHashMap/BST 的详细任务仍在 [README 第 9 节](README.md#9-手写底层结构额外约-46-小时)。

建议先完成哈希表和 BST，再做 List、Heap、RingBuffer；Array 可以作为短练习穿插。Deque 和红黑树各自安排独立学习时间，全部手写任务不包含在原 L04 的 2 小时预算内。

## 2. 构建和分阶段运行

在项目根目录构建 L04 的全部八套脚手架（不包含 L03 MiniVector）：

```sh
cmake --preset debug
cmake --build --preset debug --target container_exercises
```

只练一套时，使用对应目标和 case：

```sh
cmake --build --preset debug --target mini_list_test
./build/debug/mini_list_test empty
./build/debug/mini_list_test ends
./build/debug/mini_list_test
```

传入一个 case 名只检查该组；不传参数运行全部。返回码 0 表示选中的检查通过，1 表示检查失败，2 表示参数错误。新脚手架运行全部时应为 `1 passed, 7 failed`，失败信息指出待实现的函数。修改头文件后重新构建，再运行。

这些 TODO 练习不加入 CTest；CTest 中的 container_observe 只检查标准容器观察程序。VS Code 的目标列表已经加入各练习名称。

实现完成后在正常工作的 sanitizer 环境验证，例如：

```sh
cmake --preset asan
cmake --build --preset asan --target mini_list_test
./build/asan/mini_list_test
```

通过 8 组检查后还需要解释不变量、所有权和复杂度。sanitizer 未报错只覆盖已运行的路径。Heap 的线性建堆复杂度和树的复杂度不靠计时断言验证。

## 3. MiniList：链接和双向 iterator

存储为双向循环链表，哨兵只有 prev/next，不保存 T。BasicIterator 的模板参数只区分可修改/只读访问，解引用和前后移动仍由你实现。

接口契约：insert 在指定位置前新增，返回新元素 iterator；erase 返回下一个 iterator。位置必须来自当前容器。插入保留已有 iterator/引用，删除只使目标节点的失效。空容器的 front/back/pop，以及 erase(end)，抛 out_of_range；不能解引用或递增 end，也不能递减 begin。

| case | 实现/验证目标 |
|---|---|
| empty | 初始 size=0，已提供 |
| ends | 两端插入/删除、front/back、单元素变为空 |
| iterators | begin/end、前置/后置 ++/--、const 转换、标准算法可用性 |
| insert_erase | 中间与尾部操作、返回值、未删除节点的句柄稳定性 |
| errors | 空操作和 erase(end) 的边界行为 |
| lifetimes | 哨兵不构造 T；clear 和析构销毁所有真实节点；iterator 的箭头操作 |
| exceptions | 拷贝构造失败时链接、size、内容不变，新节点存储被回收 |
| random | 与 std::list 的随机操作结果对照，seed=406 |

先设计 clear，再完成最少的两端操作与访问，然后实现 iterator 和中间增删。构造成功前不要把新节点加入链。复制/移动容器、splice 和 list 排序留到扩展，不用一次补齐。

## 4. MiniHeap：复用存储，自己维护堆序

std::vector<T> 管理元素存储，不重复实现 MiniVector。禁止在核心实现中调用 make_heap/push_heap/pop_heap/sort_heap/priority_queue；检查代码可以用它们作对照。

Compare 与 priority_queue 的方向一致：less 为最大堆，greater 为最小堆。对每个父子对，必须满足 `!compare(parent, child)`。top 返回只读引用；空堆 top/pop 抛 out_of_range。assign 替换全部元素并做 O(n) 建堆，不能靠 n 次 push 冒充线性建堆。

| case | 实现/验证目标 |
|---|---|
| empty | 初始空状态，已提供 |
| push_pop | 上浮/下沉、重复元素、负数、堆顶顺序 |
| min_heap | 自定义比较器，验证最小堆 |
| heapify | 整批建堆、移除顺序、空输入 |
| errors | 空堆访问/删除 |
| move_only | unique_ptr 元素与自定义比较器，不能偷偷拷贝元素 |
| reuse | clear 后再次插入 |
| random | 与 std::priority_queue 对照，同时逐步检查堆序，seed=407 |

debug_values 是只读诊断视图，修改容器后重新取得。第一版使用比较不抛异常、移动/交换不抛异常的测试类型；不要求自定义比较器抛异常后的强异常保证。

## 5. RingBuffer：槽位、回绕与生命周期

构造函数已分配固定容量的原始存储，没有预先构造 T。head 表示队首物理位置，size 表示活元素数量；由你完成逻辑位置到物理槽位的映射。容量必须大于零。

try_push 成功返回 true；满时返回 false，不覆盖旧元素，也不能移动传入对象。pop 销毁队首元素。front/back/pop 在空时抛 out_of_range。下标是逻辑 FIFO 下标，调用者保证下标有效。clear 销毁活元素、复位状态、保留容量。构造失败时已有内容、size、head 不变。

| case | 实现/验证目标 |
|---|---|
| empty | 初始状态与零容量拒绝，已提供 |
| fifo | 拷贝/移动追加、逻辑下标、按进入顺序弹出 |
| wrap | 容量 1、3、5 下反复回绕，不假设容量为二的幂 |
| full_reject | 满时保留输入 unique_ptr，释放一个槽后可重试 |
| errors | 空访问/删除 |
| lifetimes | 未使用槽位没有 T；pop/clear/析构计数平衡，幸存元素地址稳定 |
| exceptions | 失败构造不提交新状态，可继续使用 |
| random | 固定容量行为与 std::deque 对照，seed=408 |

先画一张“物理槽位”和“逻辑顺序”不同的图，再写回绕。这里是单线程容器，不加 atomic 或锁；并发队列继续在 L06/L07 学习。

## 6. MiniArray：固定长度、内嵌存储

元素存储已提供：N>0 是内嵌 T 数组，默认值初始化；N=0 不构造占位 T。你实现访问与遍历接口，不写 allocate/deallocate 或手动析构。默认拷贝/移动/析构由成员行为决定。

size 永远等于 N；没有 push/pop/reserve。下标访问由调用者保证范围，at 越界抛 out_of_range。N=0 时 data/begin/end 都返回 nullptr，front/back 抛 out_of_range，fill 不做赋值；不要对空指针做算术。

| case | 实现/验证目标 |
|---|---|
| empty | 固定 extent 和 N=0 状态，已提供 |
| access | 默认初始化、const 访问、连续地址 |
| iterators | 指针 iterator、范围 for、std::sort |
| fill | 对每个已有元素赋值 |
| bounds | at 与 const at 的越界行为 |
| zero | N=0 的访问和空区间 |
| copy_move | 值拷贝独立，默认移动支持 unique_ptr |
| lifetimes | N 个元素立即存在；N=0 不构造隐藏对象 |

本练习使用 class 外壳和值初始化，未要求复刻 std::array 的聚合初始化等全部属性。

## 7. MiniDeque：分块存储与两端增长

每块拥有 BlockSize 个 T 的原始存储，std::vector<T*> 只保存块目录。禁止用 vector<T>/std::deque<T> 存储实际元素。BlockSize 必须为正；模板参数让你用 1、2、3、4 的小块快速碰到边界。

first_block/first_offset 表示首个逻辑元素的位置。块中空余槽位没有 T。两端新增和目录增长不能移动原有元素，已有元素地址保持有效；pop 只使被删除元素的失效。at 越界及空 front/back/pop 抛 out_of_range。构造失败时已有元素和 size 不变，已申请的空块可以保留，但必须由容器最终释放。clear 释放全部块并复位目录和位置。

| case | 实现/验证目标 |
|---|---|
| empty | 初始空状态，已提供 |
| ends | 两端增删、访问、const 重载、空后复用 |
| boundaries | 跨多个块、前后同时增长、BlockSize=1 |
| stability | 目录增长后保存的元素指针仍指向原元素 |
| errors | 空操作与 at 越界 |
| lifetimes | 空槽无 T、两端销毁、clear、析构、move-only 元素 |
| exceptions | 两端新块上的构造失败后仍可使用，且无泄漏 |
| random | 与 std::deque 对照，seed=409 |

先实现单端跨块，再补另一端、目录扩展和逻辑下标转换。第一版不提供 iterator、中间 insert/erase 或完整 STL 的性能保证；不要把它当作 RingBuffer 换个名字。

## 8. MiniRBTreeMap：搜索树基础上的平衡修复

先完成普通 BST，再开始这里。key/value 固定为 int/string；所有空子树使用同一个黑色 NIL。节点布局、NIL 初始状态，以及旋转/移植/修复的辅助函数接口已提供，函数体仍是 TODO。

insert 重复 key 返回 false 并保留原值，find 不插入，lower_bound_key 返回最小的 >= key 的 key 或 nullopt，items 返回有序快照。新增保持原元素地址；基线约定每次 erase 后重新获取指针。clear 释放真实节点并恢复空树，不能释放作为成员存在的 NIL。复制/移动容器暂时禁用。

每次操作后维持：BST 顺序、正确的父子链接、黑根和黑 NIL、红节点的孩子为黑、每条到后代 NIL 的路径黑高相同。NIL 左右链接始终指向自身；删除修复期间其 parent 可以用于记录父节点，clear 后恢复自环。

| case | 实现/验证目标 |
|---|---|
| empty | 黑色 NIL 与空根，已提供 |
| insert_find | 新增、重复 key、const/非 const 查找、地址稳定性 |
| lower_traversal | 边界查询、遍历、快照独立性 |
| rotations | 不同插入方向与重新着色，逐步检查性质 |
| sorted_growth | 升序/降序插入 256 个 key，逐步验证红黑性质 |
| erase | 升序、降序和打乱顺序删除到空，删除后仍满足全部性质 |
| clear_reuse | 清空、重复清空、再次插入 |
| random | 与 std::map 对照并检查真实节点，seed=411 |

[mini_rb_tree_map_test.cpp](mini_rb_tree_map_test.cpp) 中的独立验证器通过 debug_root/debug_nil 读取结构，不调用你实现的“验证通过”函数，也不固定一种树形。诊断入口只允许读取，调用者不得修改节点。

实现顺序：查找/遍历/clear → 画旋转前后图 → 插入和插入修复 → 删除和删除修复。辅助函数可以自行调整，但保持公开检查接口。分配失败注入和全部分支覆盖仍是后续扩展。

## 9. 这些脚手架的共同边界

- [check_runner.hpp](check_runner.hpp) 负责 case 选择和失败报告；[lifetime_probe.hpp](lifetime_probe.hpp) 提供活对象计数与拷贝失败注入，都不包含容器算法。
- 原始节点/存储容器的 clear 是必须完成的核心函数；只有空体时不能认为资源管理已经完成。
- 分配、元素构造可能失败，不要给所有操作一律加 noexcept。析构与 clear 假设 T 的析构不抛异常。
- 不检查固定增长倍率、桶素数、树形或某一次运行耗时；检查内容、状态、生命周期、地址稳定性和结构不变量。
- stack/queue 可在这些底层结构完成后做适配器练习；set 可复用树/哈希节点思想；LRU 可组合链表和哈希表。它们不需要在第一轮各自重写一套存储。
