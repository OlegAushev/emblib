#include <emb/can/node_link.hpp>
#include <emb/can/rolling_counter.hpp>

#include <chrono>
#include <cstdint>

namespace {

using namespace emb::can;
using namespace std::chrono_literals;
using ms = std::chrono::milliseconds;

struct frame {
  std::uint8_t data : 6;
  std::uint8_t counter : 2;
};

// The first frame carries zero, and the counter wraps to zero after three.
consteval bool test_sequence()
{
  rolling_counter<2> counter;
  frame f{};
  for (unsigned k = 0; k < 10; ++k) {
    counter.stamp(f);
    if (f.counter != k % 4) return false;
  }
  return true;
}

static_assert(test_sequence());

// Stamping leaves the rest of the frame alone.
consteval bool test_other_fields()
{
  rolling_counter<2> counter;
  frame f{.data = 0x2A, .counter = 0};
  for (unsigned k = 0; k < 4; ++k) {
    counter.stamp(f);
  }
  return f.data == 0x2A;
}

static_assert(test_other_fields());

struct wide_frame {
  std::uint64_t data : 60;
  std::uint64_t counter : 4;
};

// A wider counter wraps after its own largest value, in a field of any type.
consteval bool test_counter_width()
{
  rolling_counter<4> counter;
  wide_frame f{};
  for (unsigned k = 0; k < 40; ++k) {
    counter.stamp(f);
    if (f.counter != k % 16) return false;
  }
  return true;
}

static_assert(test_counter_width());

// A stream the counter numbers passes the check of a link of the same width,
// across the wraps.
consteval bool test_link_accepts()
{
  constexpr lost_timing timing{.timeout = 300ms,
                               .to_error = 100ms,
                               .to_critical = 200ms};
  node_link<1> link({timing}, 0ms);
  rolling_counter<2> counter;
  frame f{};
  for (unsigned k = 1; k <= 20; ++k) {
    counter.stamp(f);
    if (!link.counter_ok<1>(f.counter, ms{10 * k})) return false;
    link.report_valid<1>(ms{10 * k});
  }
  return true;
}

static_assert(test_link_accepts());

} // namespace
