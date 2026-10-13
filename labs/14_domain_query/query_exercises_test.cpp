#include "lab_check.hpp"
#include "lab_exercise_checks.hpp"
#include "query_exercises.hpp"
#include <array>
#include <limits>
#include <numeric>
#include <random>
#include <string>
#include <vector>

namespace {
using namespace domain_query;
constexpr std::array<Record, 4> records{{{10, 1, 100}, {20, 3, 200}, {30, 4, 300}, {40, 2, 400}}};

template <typename Exception, typename Function>
void throws(Function operation) {
  bool rejected = false;
  try {
    operation();
  } catch (const Exception&) {
    rejected = true;
  }
  require(rejected, "operation throws the specified exception");
}

struct OwnedColumns {
  std::vector<std::uint64_t> prices, quantities, timestamps;

  explicit OwnedColumns(std::span<const Record> rows) {
    for (const auto& row : rows) {
      prices.push_back(row.price);
      quantities.push_back(row.quantity);
      timestamps.push_back(row.timestamp);
    }
  }

  Columns view() const {
    return {prices, quantities, timestamps};
  }
};

QueryPlan filtered_sum() {
  return {Aggregate::sum,
          Field::price,
          {{Field::quantity, Compare::greater_equal, 2}, {Field::timestamp, Compare::less, 350}}};
}

}  // namespace

int main(int argc, char** argv) {
  LabExerciseChecks checks(argc, argv);
  checks.run("tokens", [] {
    const auto tokens = tokenize("SUM price WHERE quantity>=2");
    const std::array<std::string_view, 6> text{"SUM", "price", "WHERE", "quantity", ">=", "2"};
    const std::array<std::size_t, 6> offsets{0, 4, 10, 16, 24, 26};
    require(tokens.size() == text.size(), "adjacent operator and integer are separate tokens");
    for (std::size_t index = 0; index < tokens.size(); ++index) {
      require(tokens[index].text == text[index] && tokens[index].offset == offsets[index],
              "token text and source offset");
      const auto kind = index < 4    ? TokenKind::word
                        : index == 4 ? TokenKind::comparison
                                     : TokenKind::integer;
      require(tokens[index].kind == kind, "token category");
    }
  });
  checks.run("token_whitespace", [] {
    require(tokenize(" \t\r\n\v\f").empty(), "all ASCII whitespace is ignored");
    require(tokenize("").empty(), "empty input has no tokens");
    const auto tokens = tokenize(" \tCOUNT\nWHERE price!=0\r");
    require(tokens.size() == 5 && tokens[0].offset == 2 && tokens[1].offset == 8 &&
                tokens[2].offset == 14 && tokens[3].offset == 19 && tokens[4].offset == 21,
            "offsets refer to original bytes rather than trimmed input");
    const auto unknown = tokenize("unknown_field");
    require(unknown.size() == 1 && unknown[0].kind == TokenKind::word,
            "lexer does not decide which fields exist");
    const auto joined = tokenize("SUMprice");
    require(joined.size() == 1 && joined[0].text == "SUMprice", "words cannot split at a keyword");
    std::string source = "COUNT";
    const auto owned = tokenize(source);
    source.assign(100, 'x');
    require(owned.size() == 1 && owned[0].text == "COUNT", "token text owns its storage");
  });
  checks.run("token_errors", [] {
    for (std::string_view source :
         {"@", "price = 1", "price ! 1", "(price < 1)", "price + 1", "price > -1", "price > 1.5"}) {
      throws<std::invalid_argument>([&] {
        (void)tokenize(source);
      });
    }
    const std::string embedded_null("COUNT\0WHERE", 11);
    throws<std::invalid_argument>([&] {
      (void)tokenize(embedded_null);
    });
  });
  checks.run("parse_aggregate", [] {
    const auto sum = compile_query("SUM price");
    require(sum.aggregate == Aggregate::sum && sum.field == Field::price && sum.predicates.empty(),
            "SUM has a field and optional WHERE");
    const auto count = compile_query("COUNT");
    require(count.aggregate == Aggregate::count && count.predicates.empty(),
            "COUNT has no required field");
    require(compile_query("SUM quantity").field == Field::quantity &&
                compile_query("SUM timestamp").field == Field::timestamp,
            "all three fields can be aggregated");
  });
  checks.run("parse_where", [] {
    const auto plan = compile_query("SUM price WHERE quantity>=2 AND timestamp<350");
    require(plan.predicates.size() == 2, "AND produces two conditions");
    require(plan.predicates[0].field == Field::quantity &&
                plan.predicates[0].compare == Compare::greater_equal &&
                plan.predicates[0].literal == 2 && plan.predicates[1].field == Field::timestamp &&
                plan.predicates[1].compare == Compare::less && plan.predicates[1].literal == 350,
            "plan preserves field, comparison and literal");
    const auto count = compile_query("COUNT WHERE price!=0");
    require(count.aggregate == Aggregate::count && count.predicates.size() == 1 &&
                count.predicates[0].compare == Compare::not_equal,
            "COUNT supports the same predicates");
  });
  checks.run("syntax_errors", [] {
    for (std::string_view source :
         {"", " ", "SUM", "SUMprice", "COUNT price", "sum price", "SUM PRICE", "SUM unknown",
          "COUNT WHERE", "COUNT WHERE price", "COUNT WHERE price >=", "COUNT WHERE price <> 1",
          "COUNT WHERE price > -1", "COUNT WHERE price > 1 OR price < 3",
          "COUNT WHERE price > 1 AND", "COUNT WHERE price > 1 extra", "COUNT WHERE price > 1 2",
          "SUM price;"}) {
      throws<std::invalid_argument>([&] {
        (void)compile_query(source);
      });
    }
  });
  checks.run("literal_limits", [] {
    const auto maximum = compile_query("COUNT WHERE timestamp == 18446744073709551615");
    require(maximum.predicates.size() == 1 &&
                maximum.predicates[0].literal == std::numeric_limits<std::uint64_t>::max(),
            "uint64_t maximum is a valid integer literal");
    const auto zero = compile_query("COUNT WHERE price == 000");
    require(zero.predicates.size() == 1 && zero.predicates[0].literal == 0,
            "leading zeroes remain decimal");
    throws<std::invalid_argument>([] {
      (void)compile_query("COUNT WHERE price == 18446744073709551616");
    });
  });
  checks.run("plan_lifetime", [] {
    QueryPlan plan;
    {
      std::string input = "SUM quantity WHERE timestamp <= 200";
      plan = compile_query(input);
      input.assign(100, 'x');
    }
    require(plan.aggregate == Aggregate::sum && plan.field == Field::quantity &&
                plan.predicates.size() == 1 && plan.predicates[0].literal == 200 &&
                execute_aos(records, plan) == 4,
            "compiled plan remains usable after the query text is destroyed");
  });
  checks.run("comparisons", [] {
    const Record row{10, 20, 30};
    const std::array<Compare, 6> operators{Compare::equal,   Compare::not_equal,
                                           Compare::less,    Compare::less_equal,
                                           Compare::greater, Compare::greater_equal};
    const std::array<bool, 6> below{false, true, true, true, false, false};
    const std::array<bool, 6> equal{true, false, false, true, false, true};
    const std::array<bool, 6> above{false, true, false, false, true, true};
    for (std::size_t index = 0; index < operators.size(); ++index) {
      require(matches(row, {Field::price, operators[index], 11}) == below[index] &&
                  matches(row, {Field::price, operators[index], 10}) == equal[index] &&
                  matches(row, {Field::price, operators[index], 9}) == above[index],
              "all comparison operators handle less/equal/greater cases");
    }
    require(matches(row, {Field::quantity, Compare::equal, 20}) &&
                matches(row, {Field::timestamp, Compare::greater_equal, 30}),
            "predicate reads the requested field");
    const Record wide{std::numeric_limits<std::uint64_t>::max(), 0, 0};
    require(matches(wide, {Field::price, Compare::greater, 1}), "comparison remains unsigned");
  });
  checks.run("aos", [] {
    auto plan = filtered_sum();
    require(execute_aos(records, plan) == 50, "filter precedes SUM");
    plan.aggregate = Aggregate::count;
    plan.field = Field::timestamp;
    require(execute_aos(records, plan) == 2, "COUNT ignores the aggregation field");
    require(execute_aos(records, {Aggregate::sum, Field::quantity, {}}) == 10 &&
                execute_aos(records, {Aggregate::sum, Field::timestamp, {}}) == 1000 &&
                execute_aos(records, {Aggregate::count, Field::price, {}}) == 4,
            "no WHERE matches all rows");
    plan.predicates.push_back({Field::price, Compare::greater, 100});
    require(execute_aos(records, plan) == 0 && execute_aos({}, filtered_sum()) == 0,
            "no matches and empty data have zero aggregates");
  });
  checks.run("soa_batches", [] {
    const OwnedColumns columns(records);
    for (std::size_t batch : {std::size_t{1}, std::size_t{2}, std::size_t{3}, std::size_t{4},
                              std::size_t{99}, std::numeric_limits<std::size_t>::max()}) {
      require(execute_soa(columns.view(), filtered_sum(), batch) == 50,
              "all batch sizes preserve filtered SUM, including a partial final batch");
      auto count = filtered_sum();
      count.aggregate = Aggregate::count;
      require(execute_soa(columns.view(), count, batch) == 2, "batch COUNT agrees");
    }
    require(execute_soa({}, filtered_sum(), 2) == 0, "valid empty columns return zero");
  });
  checks.run("column_errors", [] {
    const std::array<std::uint64_t, 1> one{10};
    throws<std::invalid_argument>([&] {
      (void)execute_soa({one, {}, one}, filtered_sum(), 1);
    });
    throws<std::invalid_argument>([&] {
      (void)execute_soa({{}, one, {}}, filtered_sum(), 1);
    });
    throws<std::invalid_argument>([&] {
      (void)execute_soa({}, filtered_sum(), 0);
    });
  });
  checks.run("sum_overflow", [] {
    const std::array<Record, 2> rows{
        {{std::numeric_limits<std::uint64_t>::max(), 0, 0}, {1, 1, 0}}};
    const QueryPlan sum{Aggregate::sum, Field::price, {}};
    const OwnedColumns columns(rows);
    throws<std::overflow_error>([&] {
      (void)execute_aos(rows, sum);
    });
    throws<std::overflow_error>([&] {
      (void)execute_soa(columns.view(), sum, 1);
    });
    const QueryPlan filtered{
        Aggregate::sum, Field::price, {{Field::quantity, Compare::greater, 0}}};
    require(execute_aos(rows, filtered) == 1 && execute_soa(columns.view(), filtered, 1) == 1,
            "excluded rows do not contribute overflow");
    const QueryPlan count{Aggregate::count, Field::price, {}};
    require(execute_aos(rows, count) == 2 && execute_soa(columns.view(), count, 1) == 2,
            "COUNT never sums the field values");
  });
  checks.run("end_to_end", [] {
    const OwnedColumns columns(records);
    for (std::string_view query : {"SUM price WHERE quantity >= 2 AND timestamp < 350",
                                   "COUNT WHERE price >= 20", "SUM timestamp"}) {
      const auto plan = compile_query(query);
      const auto expected = query.starts_with("SUM price") ? 50U
                            : query.starts_with("COUNT")   ? 3U
                                                           : 1000U;
      require(execute_aos(records, plan) == expected &&
                  execute_soa(columns.view(), plan, 3) == expected,
              "text, plan and both execution layouts agree");
    }
  });
  checks.run("random_reference", [] {
    std::mt19937 random(42);
    std::vector<Record> rows(257);
    for (std::size_t index = 0; index < rows.size(); ++index)
      rows[index] = {random() % 1000U, random() % 5U, index};
    const OwnedColumns columns(rows);
    const auto expected = std::accumulate(
        rows.begin(), rows.end(), std::uint64_t{0}, [](std::uint64_t total, const Record& row) {
          return total + (row.quantity >= 2 && row.timestamp < 130 ? row.price : 0);
        });
    const auto plan = compile_query("SUM price WHERE quantity >= 2 AND timestamp < 130");
    require(execute_aos(rows, plan) == expected &&
                execute_soa(columns.view(), plan, 17) == expected,
            "fixed-seed data agrees with an independent fixed-query oracle");
  });
  return checks.finish();
}
