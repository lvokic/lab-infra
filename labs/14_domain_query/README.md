# L14｜领域专用编程：小型查询 DSL 与列式执行（项目专项 5h）

先完成 L09 的 AoS/SoA 与访问路径；理解 std::span、vector、字符串所有权和 uint64_t 溢出。
这项训练连接你的 CXL-Vector／DiSPQ 查询项目。它是项目表达与组件设计拓展，不替代并发、内存、网络这些面试主线，也不计入原四周 100h。

## 1. 这里的 domain-specific 是什么（20min）

领域专用编程围绕一类具体问题设计表达方式与执行组件。DSL 是领域专用语言，例如 SQL 面向数据查询，正则表达式面向模式匹配。
本实验选择小型查询语言，把“要查什么”与“如何访问数据”分别表示。定义与内部/外部 DSL 的区分见 [Martin Fowler：Domain Specific Language](https://martinfowler.com/bliki/DomainSpecificLanguage.html)。

这里写外部文本 DSL，核心任务是：

```text
查询字符串 → tokens → QueryPlan → 过滤 → 聚合结果
                                  ├─ AoS 行式执行
                                  └─ SoA 分批执行
```

token 只识别字符类别；编译查询判断语法、字段和整数范围；执行阶段消费计划与数据，不重新解析字符串。
领域适配的收益由实际工作量验证，不能因为接口名字像 SQL 就声称快。

## 2. 定点材料（25min）

| 材料 | 阅读位置 | 对应问题 |
|---|---|---|
| [Fowler DSL](https://martinfowler.com/bliki/DomainSpecificLanguage.html) | 内部 DSL 与外部 DSL 的定义 | 一个链式 C++ API 与一段独立查询文本有什么区别？ |
| [LLVM 教程：Lexer](https://llvm.org/docs/tutorial/MyFirstLanguageFrontend/LangImpl01.html) | The Lexer 一节 | 为什么词法识别与语法识别是不同阶段？只学分层，不照抄代码 |
| [LLVM 教程：Parser](https://llvm.org/docs/tutorial/MyFirstLanguageFrontend/LangImpl02.html) | parser/AST 概念 | 计划为什么可以在原文本销毁后仍存在？本实验只需受限语法 |
| [DuckDB Execution Format](https://duckdb.org/docs/current/internals/vector) | vectors 与 DataChunks | 列和批次如何交给执行算子？本实验不复刻 DuckDB |
| [L09](../09_locality/README.md) | AoS/SoA、permutation、工作集、计时限制 | 只读部分字段时参与计算的字节与可能进入缓存的字节有何不同？ |

无需先学习完整编译器、LLVM API 或 SQL 标准。批处理表示分组处理行，不自动等于 SIMD。

## 3. 已完成观察（25min）

```bash
cmake --preset debug &&
cmake --build --preset debug --target domain_query_observe --parallel 2 &&
./build/debug/domain_query_observe all
```

| case | 内容 | 要解释什么 |
|---|---|---|
| plan | SUM price WHERE quantity >= 2 AND timestamp < 350 | 哪两行匹配？为什么最后结果为 50？ |
| layout | 同一批记录的行式布局与三列布局 | SUM price 与增加 WHERE 后分别需要哪些字段？ |
| batch | 五个值按每批两行聚合 | 最后一批为何只有一行？总结果为何不变？ |

plan 的 tokens 与计划说明是手工准备的例子，过滤/聚合使用标准算法作为 oracle。
它没有调用学员 parser，也没有提供 tokenize、compile_query 或通用执行器的答案。
layout 打印实际 sizeof 和容器数据字节数，不直接测量 cache miss。

```bash
ctest --test-dir build/debug -R '^domain_query_observe$' --output-on-failure
```

## 4. 语言契约：先按这个范围实现

```text
query     := SUM field [WHERE predicate {AND predicate}]
           | COUNT [WHERE predicate {AND predicate}]
predicate := field comparison integer
field     := price | quantity | timestamp
comparison:= == | != | < | <= | > | >=
integer   := 一个或多个 ASCII 十进制数字
```

方括号表示可选部分，花括号表示重复；它们不属于输入字符。关键字严格大写，字段严格小写。
例如：

```text
SUM price
COUNT
COUNT WHERE quantity == 0
SUM timestamp WHERE price >= 20 AND quantity != 0
```

没有负数、浮点、字符串常量、括号、OR、ORDER BY、分号或注释。数字允许前导零，范围是 uint64_t。
完整消费输入；不要接受合法前缀后面的多余内容。

词法单词为 `[A-Za-z_][A-Za-z_0-9]*`，数字为 `[0-9]+`；忽略空格、tab、换行等 ASCII 空白。
`quantity>=2` 可以没有空白；`SUMprice` 是一个单词，不能拆成 SUM 和 price。
token.text 拥有字符，offset 记录其首字节在原输入中的位置。
非法字符或孤立的 `=`/`!` 在 tokenize 抛 invalid_argument；空输入的 tokens 为空，但不能编译成查询。
未知单词、语法错误和超出 uint64_t 的字面量在 compile_query 抛 invalid_argument。

QueryPlan 存聚合方式、字段和一组 AND 条件，不保存指向输入文本的 string_view。
接口使用定义好的枚举成员；不要求处理强制转换出来的非法枚举值。

## 5. 文件与五个实现步骤（120min）

| 文件 | 当前内容 |
|---|---|
| [observe.cpp](observe.cpp) | 三组标准算法观察，已完成 |
| [query_exercises.hpp](query_exercises.hpp) | tokens、计划、行与列的数据结构，五个函数均为 TODO |
| [query_exercises_test.cpp](query_exercises_test.cpp) | 15 组边界、语法、生命周期、布局和完整路径检查 |
| [benchmark.cpp](benchmark.cpp) | 测量你的两个执行器，查询只编译一次 |

### A：tokenize

先手工给 `SUM price WHERE quantity>=2` 标出 token 边界和首字节位置。
区分单词、数字和操作符；对 `>=` 等优先识别合法双字符操作符。
tokenize 不判断 SUM/字段的含义，不把数字转换成 uint64_t。通过 tokens、token_whitespace、token_errors。

### B：compile_query

先处理 SUM/COUNT 的开头，再处理可选 WHERE 和重复 AND 条件。
在纸上记录当前 token 下标与下一项应是什么；读取前检查是否还有 token，结束时检查是否全部消费。
数字转换可定点阅读 std::from_chars 的接口，必须检查转换结果与完整消费，不接受截断或溢出。
通过 parse_aggregate、parse_where、syntax_errors、literal_limits。

### C：matches

只对一个 Predicate 选择字段并作比较；先推演小于/相等/大于三种情况，覆盖六种操作符。
所有字段和字面量为 uint64_t，不能先转 int 再比较。通过 comparisons。

### D：execute_aos

逐条记录应用计划的 AND 条件，然后作 SUM 或 COUNT。无条件时全匹配，无匹配行或空输入返回 0。
COUNT 是行数，忽略 plan.field；SUM 加法溢出抛 overflow_error，未匹配的行不参与溢出判断。
通过 aos、plan_lifetime、sum_overflow 中的行式部分。

### E：execute_soa

先检查三列等长和 batch_size 非零，即使输入为空也检查。按批次遍历行号，最后一批可以不足 batch_size。
直接从需要的列读取值，不为每行重建 Record，也不在执行循环解析文本。SUM 和 COUNT 的语义与 AoS 完全相同。
考虑 batch_size 大于行数甚至为 SIZE_MAX 时怎样安全推进边界；不要依赖可能溢出的边界加法。
通过 soa_batches、column_errors、sum_overflow、end_to_end、random_reference。

这里的批次是行号区间，不需要另做线程池或网络队列。Columns 的 span 不拥有数据，调用者负责保持列存活。

## 6. 检查与计时（65min）

```bash
cmake --preset debug &&
cmake --build --preset debug --target domain_query_test --parallel 2 &&
./build/debug/domain_query_test --help

./build/debug/domain_query_test tokens
./build/debug/domain_query_test parse_where
./build/debug/domain_query_test end_to_end
./build/debug/domain_query_test all
```

初始 15 项会明确报 TODO；检查不加入默认 CTest，观察程序则独立通过。
未知 case 或多余参数退出 2，检查失败退出 1，全部成功退出 0。不要移除错误输入来绕过检查。

```bash
cmake --preset asan &&
cmake --build --preset asan --target domain_query_test --parallel 2 &&
./build/asan/domain_query_test all
```

实现通过后测量：

```bash
cmake --preset release &&
cmake --build --preset release --target domain_query_benchmark --parallel 2 &&
./build/release/domain_query_benchmark
./build/release/domain_query_benchmark --batch 1
./build/release/domain_query_benchmark --batch 256
```

固定 32768 行，batch 接受 1..4096；数据约 1.5 MiB，取决于实际 sizeof。
构造数据、转成列、编译查询不计入执行计时；warmup 后按交替顺序测三轮，核对同一个查询的结果。
不存在 TODO 时运行参考实现的退路，不断言列式或批处理必须更快。
这里只比较相同聚合语义的执行路径，没有测解析吞吐、网络延迟、cache miss 或 SIMD 指令数量。

## 7. 验收与面试追问（20min）

15 项检查通过；画一次字符串→token→plan→过滤→聚合的数据流；解释字符串和列数据的所有权。
记录执行次数、字节数、batch、checksum、编译模式及三轮耗时，并回答：

1. DSL 与领域专用 C++ API 分别解决什么问题？什么时候一个普通函数接口就足够？
2. 为什么只识别单词不等于判断语法合法？为何必须拒绝未消费的尾部输入？
3. 为什么计划不保存指向临时查询字符串的视图？列的 span 又由谁拥有？
4. WHERE 只用 quantity、SUM 只用 price，timestamp 列是否需要参与计算？
5. 改为查询所有字段时，AoS/SoA 的取舍如何变化？增大 batch 如何影响工作集？
6. 哪些结果可以连接你真实的 CXL/DiSPQ 测量，哪些只是当前机器的小模型？

下一阶段可讨论算子融合、predicate pushdown、selection vector 与 SIMD，但须先完成当前正确性与测量。
完整 SQL、代码生成/JIT 和查询优化器不属于这项五小时训练。
