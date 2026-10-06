#include <emb/can/canopen/node_link.hpp>

#include <chrono>

namespace {

using namespace emb::can::canopen;
using namespace std::chrono_literals;
using ms = std::chrono::milliseconds;

constexpr lost_timing timing{.to_error = 100ms, .to_critical = 200ms};

// Hands frame 1 to the link as a handler would and returns whether it was
// taken as valid.
constexpr bool deliver(node_link<1>& link, ms now, unsigned counter)
{
  bool const ok = link.counter_ok<1>(counter, now);
  if (ok) {
    link.report_valid<1>(now);
  }
  return ok;
}

// Frame k arrives at 10k ms carrying counter k, wrapping past 3.
consteval bool test_steady_stream()
{
  node_link<1> link(timing, {300ms}, 0ms);
  for (unsigned k = 1; k <= 100; ++k) {
    if (!deliver(link, ms{10 * k}, k)) return false;
    if (link.loss(ms{10 * k}) != link_loss::none) return false;
  }
  return true;
}

static_assert(test_steady_stream());

// The sender's application stops at 100 ms while its last frame keeps going
// out: not one repeat is taken, and the link is lost from the deadline of the
// last frame that moved the counter.
consteval bool test_frozen_counter()
{
  node_link<1> link(timing, {300ms}, 0ms);
  for (unsigned k = 1; k <= 10; ++k) {
    deliver(link, ms{10 * k}, k);
  }
  for (unsigned k = 11; k <= 200; ++k) {
    if (deliver(link, ms{10 * k}, 10)) return false;
  }
  return link.loss(399ms) == link_loss::none
      && link.loss(400ms) == link_loss::warning
      && link.loss(500ms) == link_loss::error
      && link.loss(600ms) == link_loss::critical
      && link.loss(2000ms) == link_loss::critical;
}

static_assert(test_frozen_counter());

// One frame lost on the bus costs the one after it, and the link stays up.
consteval bool test_lost_frame()
{
  node_link<1> link(timing, {300ms}, 0ms);
  for (unsigned k = 1; k <= 10; ++k) {
    deliver(link, ms{10 * k}, k);
  }
  if (deliver(link, 120ms, 12)) return false;
  for (unsigned k = 13; k <= 22; ++k) {
    if (!deliver(link, ms{10 * k}, k)) return false;
  }
  return link.loss(220ms) == link_loss::none;
}

static_assert(test_lost_frame());

// The bus falls silent from 500 to 1200 ms; the stream comes back with
// whatever counter the sender has reached.
consteval bool test_silence()
{
  node_link<1> link(timing, {300ms}, 0ms);
  for (unsigned k = 1; k <= 50; ++k) {
    deliver(link, ms{10 * k}, k);
  }
  if (link.loss(799ms) != link_loss::none
      || link.loss(800ms) != link_loss::warning
      || link.loss(900ms) != link_loss::error
      || link.loss(1000ms) != link_loss::critical) {
    return false;
  }
  return deliver(link, 1200ms, 7) && link.loss(1200ms) == link_loss::none;
}

static_assert(test_silence());

// Nothing is missed before it has been heard; frame 1 is never sent.
consteval bool test_from_first_frame()
{
  node_link<2> link(from_first_frame, timing, {300ms, 300ms});
  if (link.loss(10'000ms) != link_loss::none) return false;
  if (!link.counter_ok<2>(3u, 10'000ms)) return false;
  link.report_valid<2>(10'000ms);
  return link.loss(10'299ms) == link_loss::none
      && link.loss(10'300ms) == link_loss::warning;
}

static_assert(test_from_first_frame());

// Frame 1 stops at 100 ms while frame 2 keeps arriving.
consteval bool test_earliest_deadline()
{
  node_link<2> link(timing, {300ms, 1000ms}, 0ms);
  link.counter_ok<1>(1u, 100ms);
  link.report_valid<1>(100ms);
  for (unsigned k = 1; k <= 100; ++k) {
    if (!link.counter_ok<2>(k, ms{10 * k})) return false;
    link.report_valid<2>(ms{10 * k});
  }
  return link.loss(399ms) == link_loss::none
      && link.loss(400ms) == link_loss::warning;
}

static_assert(test_earliest_deadline());

consteval bool test_counter_width()
{
  node_link<1, 4> link(timing, {300ms}, 0ms);
  return link.counter_ok<1>(15u, 10ms)
      && link.counter_ok<1>(16u, 20ms)
      && !link.counter_ok<1>(2u, 30ms);
}

static_assert(test_counter_width());

consteval bool test_never()
{
  node_link<1> link({.to_error = 100ms, .to_critical = never}, {300ms}, 0ms);
  return link.loss(400ms) == link_loss::error
      && link.loss(1'000'000ms) == link_loss::error;
}

static_assert(test_never());

} // namespace
