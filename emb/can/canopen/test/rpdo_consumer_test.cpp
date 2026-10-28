#include <emb/can/canopen/detail/rpdo_consumer.hpp>
#include <emb/can/canopen/types.hpp>

#include <chrono>
#include <cstdint>
#include <expected>

#include <emb/can.hpp>
#include <emb/delegate.hpp>
#include <emb/test/mock/can_bus.hpp>

namespace {

using namespace emb::can::canopen;
using namespace std::chrono_literals;
using emb::can::format_t;
using emb::can::frame_t;
using emb::can::payload_t;
using detail::rpdo_refusal;

constexpr std::unexpected unmatched{rpdo_refusal::unmatched};
constexpr std::unexpected not_operational{rpdo_refusal::not_operational};
constexpr std::unexpected length_error{rpdo_refusal::length_error};
constexpr std::unexpected repeated_length_error{
    rpdo_refusal::repeated_length_error};

// RPDO1 of node 1, set up at 0 ms with a timeout of 350 ms, whose handler
// counts the frames it takes. On its bus the consumer only sets its filter.
struct rpdo1 {
  emb::test::can_bus bus;
  detail::rpdo_consumer<1> consumer{bus, node_id{1}};
  int taken = 0;

  constexpr rpdo1()
  {
    setup(0ms);
  }

  constexpr void setup(std::chrono::milliseconds now)
  {
    consumer.setup<1, pdo_id::predefined()>(
        {.handler = emb::make_delegate<&rpdo1::take>(this),
         .timeout = 350ms,
         .on_timeout = {}},
        now);
  }

  constexpr void take(payload_t const&)
  {
    ++taken;
  }

  // Hands the consumer a frame of RPDO1 with a DLC of `len`.
  constexpr std::expected<void, rpdo_refusal>
  receive(std::uint8_t len,
          std::chrono::milliseconds now,
          nmt_state state = nmt_state::operational)
  {
    frame_t const frame{.format = format_t::standard,
                        .id = cob_id_of<cob_type::rpdo, 1>(node_id{1}),
                        .len = len,
                        .payload = {}};
    return consumer.try_handle(frame, now, state);
  }
};

// A frame of 0 to 7 bytes never reaches the handler, and each that follows a
// full frame is a length error.
consteval bool test_short_frame()
{
  rpdo1 r;
  for (std::uint8_t len = 0; len < 8; ++len) {
    if (r.receive(len, 10ms) != length_error) return false;
    if (r.taken != len) return false;
    if (!r.receive(8, 10ms)) return false;
  }
  return r.taken == 8;
}

static_assert(test_short_frame());

// A DLC of 9 to 15 means 8 bytes in classic CAN, and the frame reaches the
// handler.
consteval bool test_long_dlc()
{
  rpdo1 r;
  for (std::uint8_t len = 9; len <= 15; ++len) {
    if (!r.receive(len, 10ms)) return false;
  }
  return r.taken == 7;
}

static_assert(test_long_dlc());

// Of a run of short frames only the first is a length error, and the others
// are repeated ones; a full frame ends the run, and the next short frame is a
// length error again.
consteval bool test_run()
{
  rpdo1 r;
  return r.receive(2, 10ms) == length_error
      && r.receive(2, 20ms) == repeated_length_error
      && r.receive(0, 30ms) == repeated_length_error
      && r.taken == 0
      && r.receive(8, 40ms).has_value()
      && r.taken == 1
      && r.receive(7, 50ms) == length_error;
}

static_assert(test_run());

// Entering operational, which resets the timers, and setting the slot up
// again each end a run.
consteval bool test_run_reset()
{
  rpdo1 r;
  if (r.receive(2, 10ms) != length_error) return false;
  r.consumer.reset_timers(20ms);
  if (r.receive(2, 30ms) != length_error) return false;
  r.setup(40ms);
  return r.receive(2, 50ms) == length_error;
}

static_assert(test_run_reset());

// Outside operational a frame is refused before its length counts: it is no
// length error, and it starts no run that would hide the next one.
consteval bool test_not_operational()
{
  rpdo1 r;
  return r.receive(2, 10ms, nmt_state::pre_operational) == not_operational
      && r.receive(8, 20ms, nmt_state::stopped) == not_operational
      && r.taken == 0
      && r.receive(2, 30ms) == length_error;
}

static_assert(test_not_operational());

// Short frames do not keep the slot alive: it times out 350 ms after its last
// full frame, and a short frame after that does not make it time out again.
consteval bool test_timeout()
{
  rpdo1 r;
  if (!r.receive(8, 100ms)) return false;
  if (r.receive(2, 200ms) != length_error) return false;
  if (r.receive(2, 300ms) != repeated_length_error) return false;
  if (r.receive(2, 400ms) != repeated_length_error) return false;
  if (r.consumer.tick(449ms, nmt_state::operational) != 0) return false;
  if (r.consumer.tick(450ms, nmt_state::operational) != 1) return false;
  if (r.receive(2, 500ms) != repeated_length_error) return false;
  if (r.consumer.tick(900ms, nmt_state::operational) != 0) return false;
  if (!r.receive(8, 1000ms)) return false;
  return r.consumer.tick(1349ms, nmt_state::operational) == 0
      && r.consumer.tick(1350ms, nmt_state::operational) == 1;
}

static_assert(test_timeout());

// A frame on another COB-ID is no slot's, whatever its length, and starts no
// run.
consteval bool test_unmatched()
{
  rpdo1 r;
  frame_t const rpdo2{.format = format_t::standard,
                      .id = cob_id_of<cob_type::rpdo, 2>(node_id{1}),
                      .len = 2,
                      .payload = {}};
  auto const other = r.consumer.try_handle(rpdo2, 10ms, nmt_state::operational);
  return other == unmatched && r.receive(2, 20ms) == length_error;
}

static_assert(test_unmatched());

} // namespace
