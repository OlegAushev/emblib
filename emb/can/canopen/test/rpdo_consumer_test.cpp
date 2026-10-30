#include <emb/can/canopen/detail/rpdo_consumer.hpp>
#include <emb/can/canopen/od_process_data.hpp>
#include <emb/can/canopen/pdo.hpp>
#include <emb/can/canopen/server.hpp>
#include <emb/can/canopen/types.hpp>

#include <chrono>
#include <cstdint>
#include <expected>
#include <type_traits>

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

// -- a slot set up from the description of its PDO ---------------------------

// A flag, a signed level and a counter of two bits.
struct command {
  bool stop;
  std::int8_t level;
  std::uint8_t counter = 0;
};

// clang-format off
inline constexpr pdo_row<command> command_rows[] = {
{{ 0, 1}, "stop", {"Stop", "Стоп"},      pdo_map<&command::stop>},
{{ 8, 8}, "level", {"Level", "Уровень"}, pdo_map<&command::level>},
{{56, 2}, {}, {},                        pdo_map_counter<&command::counter>},
};
// clang-format on

inline constexpr auto command_pdo =
    pdo_of<command_rows>({.name = "command",
                          .title = {"Command", "Команда"},
                          .direction = pdo_direction::rx,
                          .number = 1,
                          .cob_id = pdo_id::predefined(),
                          .timeout = 350ms});

// RPDO1 of node 1, set up at 0 ms from the description of its PDO with the
// server's thunk, whose handler keeps what each frame says: the flag and the
// counter, which it peeks at, and the message, or why it is refused.
struct described_rpdo1 {
  emb::test::can_bus bus;
  detail::rpdo_consumer<1> consumer{bus, node_id{1}};
  int taken = 0;
  bool stop = false;
  std::uint8_t counter = 0;
  std::expected<command, pdo_decode_failure> decoded{};

  constexpr described_rpdo1()
  {
    consumer.setup<1, pdo_id::predefined()>(
        {.handler =
             detail::described_handler<command_pdo, &described_rpdo1::handle>(
                 *this),
         .timeout = command_pdo.pdo.timeout,
         .on_timeout = {}},
        0ms);
  }

  constexpr void handle(received_pdo<command_rows> const& rpdo)
  {
    ++taken;
    stop = rpdo.peek<&command::stop>();
    counter = rpdo.peek<&command::counter>();
    decoded = rpdo.decode();
  }

  constexpr std::expected<void, rpdo_refusal> receive(payload_t const& payload,
                                                      std::uint8_t len = 8)
  {
    frame_t const frame{.format = format_t::standard,
                        .id = cob_id_of<cob_type::rpdo, 1>(node_id{1}),
                        .len = len,
                        .payload = payload};
    return consumer.try_handle(frame, 10ms, nmt_state::operational);
  }
};

// The handler gets the frame with the rows of its PDO: what it peeks at and
// decodes is what peek_pdo and decode_pdo read of the frame by the rows.
consteval bool test_received()
{
  described_rpdo1 r;
  constexpr payload_t frame = {0x01, 0xFE, 0, 0, 0, 0, 0, 0x02};
  if (!r.receive(frame) || r.taken != 1) return false;
  auto const expected = decode_pdo<command_rows>(frame);
  return r.decoded
      && expected
      && r.decoded->stop
      && r.decoded->level == -2
      && r.decoded->counter == 2
      && r.decoded->level == expected->level
      && r.stop == peek_pdo<&command::stop, command_rows>(frame)
      && r.counter == peek_pdo<&command::counter, command_rows>(frame)
      && r.counter == 2;
}

static_assert(test_received());

// A frame with a reserved bit set reaches the handler, which peeks at its
// flag and its counter and learns from decode why it is refused; a short
// frame does not.
consteval bool test_received_refused()
{
  described_rpdo1 r;
  if (!r.receive({0x03, 0, 0, 0, 0, 0, 0, 0x01})) return false;
  if (r.taken != 1 || !r.stop || r.counter != 1) return false;
  if (r.decoded
      || r.decoded.error().reason != pdo_decode_refusal::reserved_bit) {
    return false;
  }
  return r.receive({0x01, 0, 0, 0, 0, 0, 0, 0}, 2) == length_error
      && r.taken == 1;
}

static_assert(test_received_refused());

// The server takes a handler of the frames of the PDO, and no other: one of
// the frames of other rows does not take them.
inline constexpr pdo_row<command> other_rows[] = {
    {{0, 1}, "stop", {"Stop", "Стоп"}, pdo_map<&command::stop>},
};

struct handlers {
  constexpr void handle(received_pdo<command_rows> const&) {}
  constexpr void handle_other(received_pdo<other_rows> const&) {}
};

static_assert(std::is_invocable_v<decltype(&handlers::handle),
                                  handlers*,
                                  received_pdo<command_rows> const&>);
static_assert(!std::is_invocable_v<decltype(&handlers::handle_other),
                                   handlers*,
                                   received_pdo<command_rows> const&>);

} // namespace
