#ifdef __cpp_constexpr

#include <emblib/tests/tests.hpp>

#include <emblib/circular_buffer.hpp>

namespace emb {
namespace internal {
namespace tests {

constexpr bool test_circular_buffer(CircularBuffer auto buf)
  requires(std::same_as<typename decltype(buf)::value_type, int>) {
  EMB_CONSTEXPR_ASSERT(buf.empty());

  buf.push_back(1);
  EMB_CONSTEXPR_ASSERT(buf.front() == 1 && buf.back() == 1 && buf.size() == 1);

  buf.pop_front();
  EMB_CONSTEXPR_ASSERT(buf.empty() && buf.size() == 0);

  buf.push_back(2);
  EMB_CONSTEXPR_ASSERT(buf.front() == 2 && buf.back() == 2 && buf.size() == 1);

  buf.push_back(3);
  EMB_CONSTEXPR_ASSERT(buf.size() == 2 && buf.back() == 3);

  buf.push_back(4);
  EMB_CONSTEXPR_ASSERT(buf.size() == 3 && !buf.full() && buf.back() == 4);

  buf.push_back(5);
  EMB_CONSTEXPR_ASSERT(buf.size() == 4 && buf.full() && buf.back() == 5);

  buf.push_back(6);
  EMB_CONSTEXPR_ASSERT(
      buf.size() == 4 && buf.full() && buf.front() == 3 && buf.back() == 6);

  buf.push_back(7);
  buf.push_back(8);
  EMB_CONSTEXPR_ASSERT(buf.front() == 5 && buf.back() == 8);

  buf.push_back(9);
  EMB_CONSTEXPR_ASSERT(buf.front() == 6 && buf.back() == 9);

  buf.pop_front();
  EMB_CONSTEXPR_ASSERT(
      buf.size() == 3 && !buf.full() && buf.front() == 7 && buf.back() == 9);

  buf.pop_front();
  EMB_CONSTEXPR_ASSERT(buf.size() == 2 && buf.front() == 8 && buf.back() == 9);

  buf.push_back(10);
  EMB_CONSTEXPR_ASSERT(buf.size() == 3 && buf.front() == 8 && buf.back() == 10);

  buf.push_back(11);
  EMB_CONSTEXPR_ASSERT(buf.full() && buf.front() == 8 && buf.back() == 11);

  buf.fill(12);
  EMB_CONSTEXPR_ASSERT(buf.full() && buf.front() == 12 && buf.back() == 12);

  buf.pop_front();
  EMB_CONSTEXPR_ASSERT(!buf.full() && buf.front() == 12 && buf.back() == 12);

  return true;
}

static_assert(test_circular_buffer(emb::circular_buffer<int, 4>{}));
static_assert(test_circular_buffer(emb::circular_buffer<int>{4}));
static_assert(test_circular_buffer(emb::v1::circular_buffer<int, 4>{}));

} // namespace tests
} // namespace internal
} // namespace emb

#endif
