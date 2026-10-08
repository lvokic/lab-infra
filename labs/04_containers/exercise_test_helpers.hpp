#pragma once
#include "lab_check.hpp"
#include <cstddef>
#include <stdexcept>
#include <string_view>

template <typename Operation>
void require_out_of_range(Operation operation, std::string_view message) {
  bool threw = false;
  try {
    operation();
  } catch (const std::out_of_range&) {
    threw = true;
  }
  require(threw, message);
}

template <typename Actual, typename Expected>
void require_indexed_equal(const Actual& actual, const Expected& expected) {
  require(actual.size() == expected.size(), "sequence sizes match");
  for (std::size_t index = 0; index < expected.size(); ++index) {
    require(actual[index] == expected[index], "logical element order matches");
  }
}
