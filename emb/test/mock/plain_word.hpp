#pragma once

#include <atomic>

#include <cstdint>

namespace emb {
namespace test {

// The class `plain_word` stands in for `std::atomic<std::uint32_t>` as the
// word type of `settings::basic_pending_changes`, so that a test runs in a
// constant expression, where `std::atomic` cannot.
struct plain_word {
  std::uint32_t value = 0;

  constexpr std::uint32_t load(std::memory_order) const
  {
    return value;
  }

  constexpr std::uint32_t fetch_or(std::uint32_t bits, std::memory_order)
  {
    auto const before = value;
    value |= bits;
    return before;
  }

  constexpr std::uint32_t fetch_and(std::uint32_t bits, std::memory_order)
  {
    auto const before = value;
    value &= bits;
    return before;
  }
};

} // namespace test
} // namespace emb
