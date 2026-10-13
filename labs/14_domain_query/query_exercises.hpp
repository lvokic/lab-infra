#pragma once
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace domain_query {

enum class TokenKind { word, integer, comparison };

// text 拥有自己的字符；offset 是 token 首字节在输入中的位置。
struct Token {
  TokenKind kind;
  std::string text;
  std::size_t offset;
};

enum class Field { price, quantity, timestamp };
enum class Compare { equal, not_equal, less, less_equal, greater, greater_equal };
enum class Aggregate { sum, count };

struct Predicate {
  Field field;
  Compare compare;
  std::uint64_t literal;
};

// 已解析的语义，独立于原查询字符串。COUNT 忽略 field；predicates 之间为 AND。
struct QueryPlan {
  Aggregate aggregate = Aggregate::count;
  Field field = Field::price;
  std::vector<Predicate> predicates;
};

struct Record {
  std::uint64_t price = 0;
  std::uint64_t quantity = 0;
  std::uint64_t timestamp = 0;
};

// 三列都由调用者拥有，且必须等长；span 的生命周期须覆盖执行过程。
struct Columns {
  std::span<const std::uint64_t> prices;
  std::span<const std::uint64_t> quantities;
  std::span<const std::uint64_t> timestamps;
};

// TODO A：ASCII 单词 [A-Za-z_][A-Za-z_0-9]*、十进制整数、六种比较操作符。
// 忽略 ASCII 空白；空输入返回空 vector；非法字符/不完整操作符抛 invalid_argument。
// 不在这里判断字段名/关键字，也不在这里将数字转成 uint64_t。
inline std::vector<Token> tokenize(std::string_view input) {
  (void)input;
  throw std::logic_error("TODO: tokenize");
}

// TODO B：完整消费 README 的小型语法，生成拥有自己数据的 QueryPlan。
// 关键字必须大写，字段必须小写。错误语法/未知字段/整数越界抛 invalid_argument。
inline QueryPlan compile_query(std::string_view input) {
  (void)input;
  throw std::logic_error("TODO: compile_query");
}

// TODO C：只判断一个比较条件；字段值与 literal 都是 uint64_t。
inline bool matches(const Record& record, const Predicate& predicate) {
  (void)record;
  (void)predicate;
  throw std::logic_error("TODO: matches");
}

// TODO D：AND 过滤后聚合。没有条件时全部匹配；空输入返回 0。
// SUM 加法溢出抛 overflow_error；COUNT 计算匹配行数，忽略 plan.field。
inline std::uint64_t execute_aos(std::span<const Record> records, const QueryPlan& plan) {
  (void)records;
  (void)plan;
  throw std::logic_error("TODO: execute_aos");
}

// TODO E：按 batch_size 分批执行同一计划，结果与 AoS 一致。
// batch_size==0 或三列不等长抛 invalid_argument，空输入也要检查参数。
// 支持尾批不足 batch_size；不为每行构造 Record，不在循环中解析查询。
// SUM 同样检查溢出。枚举值由接口提供的合法成员组成。
inline std::uint64_t execute_soa(const Columns& columns, const QueryPlan& plan,
                                 std::size_t batch_size = 64) {
  (void)columns;
  (void)plan;
  (void)batch_size;
  throw std::logic_error("TODO: execute_soa");
}

}  // namespace domain_query
