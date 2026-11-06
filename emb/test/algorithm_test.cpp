#include <emb/algorithm.hpp>

#include <algorithm>
#include <array>

namespace {

// The median of three values does not depend on their order, also with ties.
consteval bool test_median_of_three()
{
  std::array<int, 3> v{1, 2, 3};
  do {
    if (emb::median(v[0], v[1], v[2]) != 2) return false;
  } while (std::ranges::next_permutation(v).found);

  return emb::median(5, 5, 1) == 5
      && emb::median(5, 1, 5) == 5
      && emb::median(1, 5, 5) == 5
      && emb::median(1, 1, 5) == 1
      && emb::median(-1.0f, 2.0f, 0.0f) == 0.0f;
}

static_assert(test_median_of_three());

// The median of five values is that of a sort, in every order of five distinct
// values and in every window of four levels, which holds ties of every kind.
consteval bool test_median_of_five()
{
  std::array<int, 5> distinct{1, 2, 3, 4, 5};
  do {
    auto const& v = distinct;
    if (emb::median(v[0], v[1], v[2], v[3], v[4]) != 3) return false;
  } while (std::ranges::next_permutation(distinct).found);

  for (auto code = 0; code < 1024; ++code) {
    std::array<int, 5> v{};
    for (auto i = 0uz; i < v.size(); ++i) {
      v[i] = (code >> (2 * i)) & 3;
    }
    auto sorted = v;
    std::ranges::sort(sorted);
    if (emb::median(v[0], v[1], v[2], v[3], v[4]) != sorted[2]) return false;
  }

  return emb::median(-1.0f, 2.0f, 0.0f, 9.0f, -7.0f) == 0.0f;
}

static_assert(test_median_of_five());

// The median of seven values is that of a sort on every window of zeros and
// ones. It is built of compare and exchange steps, `min` and `max` alone, so by
// the 0-1 principle that holds for all values.
consteval bool test_median_of_seven()
{
  for (auto code = 0; code < 128; ++code) {
    std::array<int, 7> v{};
    for (auto i = 0uz; i < v.size(); ++i) {
      v[i] = (code >> i) & 1;
    }
    auto sorted = v;
    std::ranges::sort(sorted);
    if (emb::median(v[0], v[1], v[2], v[3], v[4], v[5], v[6]) != sorted[3]) {
      return false;
    }
  }
  return emb::median(-1.0f, 2.0f, 0.0f, 9.0f, -7.0f, 5.0f, -3.0f) == 0.0f;
}

static_assert(test_median_of_seven());

// The median of nine values, by the 0-1 principle as for seven.
consteval bool test_median_of_nine()
{
  for (auto code = 0; code < 512; ++code) {
    std::array<int, 9> v{};
    for (auto i = 0uz; i < v.size(); ++i) {
      v[i] = (code >> i) & 1;
    }
    auto sorted = v;
    std::ranges::sort(sorted);
    if (emb::median(v) != sorted[4]) return false;
    if (emb::median(v[0], v[1], v[2], v[3], v[4], v[5], v[6], v[7], v[8])
        != sorted[4]) {
      return false;
    }
  }
  return emb::median(-1.0f, 2.0f, 0.0f, 9.0f, -7.0f, 5.0f, -3.0f, 4.0f, -6.0f)
      == 0.0f;
}

static_assert(test_median_of_nine());

// An array of any odd size, of a size with an overload of its own or not.
consteval bool test_median_of_array()
{
  return emb::median(std::array{7}) == 7
      && emb::median(std::array{3, 1, 2}) == 2
      && emb::median(std::array{5, 1, 4, 2, 3}) == 3
      && emb::median(std::array{9, 1, 8, 2, 7, 3, 6}) == 6
      && emb::median(std::array{9, 1, 8, 2, 7, 3, 6, 5, 4}) == 5
      && emb::median(std::array{9, 1, 8, 2, 7, 3, 6, 5, 4, 0, 10}) == 5;
}

static_assert(test_median_of_array());

// An even number of values has no single median.
template<typename A>
concept has_median = requires(A a) { emb::median(a); };

static_assert(has_median<std::array<int, 3>>);
static_assert(!has_median<std::array<int, 4>>);

} // namespace
