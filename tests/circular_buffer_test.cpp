#ifdef __cpp_constexpr

#include <emb/circular_buffer.hpp>

namespace emb {
namespace internal {
namespace tests {

constexpr bool test_circular_buffer(circular_buffer_type auto buf)
  requires(std::same_as<typename decltype(buf)::value_type, int>) {
  int const cap{static_cast<int>(buf.capacity())};

  assert(buf.empty());

  buf.push_back(1);
  assert(buf.front() == 1 && buf.back() == 1 && buf.size() == 1);

  if (buf.capacity() != 1) {
    assert(!buf.full());
  } else {
    assert(buf.full());
  }

  buf.pop_front();
  assert(buf.empty() && buf.size() == 0);

  buf.push_back(1);
  buf.pop_back();
  assert(buf.empty() && buf.size() == 0);

  for (auto i{1}; i <= cap; ++i) {
    buf.push_back(i);
    assert(
        buf.front() == 1 && buf.back() == i &&
        buf.size() == static_cast<size_t>(i)
    );
  }

  assert(buf.size() == buf.capacity() && buf.full());

  auto val = buf.back();
  auto size = buf.size();
  while (!buf.empty()) {
    assert(buf.size() == size--);
    assert(buf.front() == 1);
    assert(buf.back() == val--);
    buf.pop_back();
  }

  for (auto i{1}; i <= cap; ++i) {
    buf.push_back(i);
  }

  val = buf.front();
  size = buf.size();
  while (!buf.empty()) {
    assert(buf.size() == size--);
    assert(buf.front() == val++);
    assert(buf.back() == cap);
    buf.pop_front();
  }

  auto const max_val{2 * cap + cap};
  for (auto i{1}; i <= max_val; ++i) {
    buf.push_back(i);
  }

  assert(buf.back() == max_val);
  assert(buf.front() == max_val - cap + 1);

  size = buf.size();
  while (buf.size() != 1) {
    assert(buf.size() == size--);
    buf.pop_back();
  }

  assert(buf.back() == max_val - cap + 1);
  assert(buf.front() == max_val - cap + 1);

  buf.pop_back();
  assert(buf.empty());

  buf.clear();
  for (auto i{1}; i <= max_val; ++i) {
    buf.push_back(i);
  }

  size = buf.size();
  while (buf.size() != 1) {
    assert(buf.size() == size--);
    buf.pop_front();
  }

  assert(buf.back() == max_val);
  assert(buf.front() == max_val);

  buf.pop_front();
  assert(buf.empty());

  buf.fill(42);
  assert(buf.full() && buf.front() == 42 && buf.back() == 42);

  buf.pop_front();
  assert(!buf.full());
  if (buf.capacity() != 1) {
    assert(buf.front() == 42 && buf.back() == 42);
  } else {
    assert(buf.empty());
  }

  return true;
}

static_assert(test_circular_buffer(emb::circular_buffer<int, 1>{}));
static_assert(test_circular_buffer(emb::circular_buffer<int>{1}));
static_assert(test_circular_buffer(emb::v1::circular_buffer<int, 1>{}));

static_assert(test_circular_buffer(emb::circular_buffer<int, 2>{}));
static_assert(test_circular_buffer(emb::circular_buffer<int>{2}));
static_assert(test_circular_buffer(emb::v1::circular_buffer<int, 2>{}));

static_assert(test_circular_buffer(emb::circular_buffer<int, 5>{}));
static_assert(test_circular_buffer(emb::circular_buffer<int>{5}));
static_assert(test_circular_buffer(emb::v1::circular_buffer<int, 5>{}));

static_assert(test_circular_buffer(emb::circular_buffer<int, 10>{}));
static_assert(test_circular_buffer(emb::circular_buffer<int>{10}));
static_assert(test_circular_buffer(emb::v1::circular_buffer<int, 10>{}));

} // namespace tests
} // namespace internal
} // namespace emb

#endif
