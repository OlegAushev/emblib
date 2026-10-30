#include <emb/can/canopen/detail/tpdo_producer.hpp>
#include <emb/can/canopen/od_process_data.hpp>
#include <emb/can/canopen/pdo.hpp>
#include <emb/can/canopen/server.hpp>
#include <emb/can/canopen/types.hpp>

#include <chrono>
#include <cstdint>

#include <emb/can.hpp>
#include <emb/delegate.hpp>
#include <emb/test/mock/can_bus.hpp>

namespace {

using namespace emb::can::canopen;
using namespace std::chrono_literals;
using emb::can::format_t;
using emb::can::payload_t;

// TPDO1 of node 1, set up at 0 ms with a period of 10 ms, whose provider
// counts its calls and puts their number in the first byte.
struct tpdo1 {
  emb::test::can_bus bus;
  detail::tpdo_producer<1> producer{bus, node_id{1}};
  int calls = 0;

  constexpr tpdo1()
  {
    producer.setup<1, pdo_id::predefined()>(
        {.provider = emb::make_delegate<&tpdo1::provide>(this), .period = 10ms},
        0ms);
  }

  constexpr payload_t provide()
  {
    ++calls;
    return {static_cast<std::uint8_t>(calls)};
  }

  // Ticks the producer once a millisecond, from `first` to `last`, as a main
  // loop that runs the server that often would.
  constexpr void run(std::chrono::milliseconds first,
                     std::chrono::milliseconds last)
  {
    for (auto now = first; now <= last; ++now) {
      producer.tick(now, nmt_state::operational);
    }
  }
};

// While the bus takes every frame, the slot sends one a period, on its COB-ID,
// 8 bytes from its provider.
consteval bool test_period()
{
  tpdo1 t;
  t.run(0ms, 100ms);
  auto const& frame = t.bus.last();
  return t.calls == 10
      && t.bus.taken == 10
      && frame.format == format_t::standard
      && frame.id == cob_id_of<cob_type::tpdo, 1>(node_id{1})
      && frame.len == 8
      && frame.payload[0] == 10;
}

static_assert(test_period());

// While the driver's queue is full, the slot calls its provider once a
// period, 10 times in 100 ms, and not on every tick.
consteval bool test_full_queue()
{
  tpdo1 t;
  t.bus.set_queue_full(true);
  t.run(0ms, 100ms);
  return t.calls == 10 && t.bus.refused == 10 && t.bus.taken == 0;
}

static_assert(test_full_queue());

// A frame that send refused is not sent again once the queue has room: the
// next frame comes a period after it, from a fresh call of the provider.
consteval bool test_refused_frame()
{
  tpdo1 t;
  t.bus.set_queue_full(true);
  t.run(0ms, 10ms);
  t.bus.set_queue_full(false);
  t.run(11ms, 19ms);
  if (t.calls != 1 || t.bus.taken != 0) return false;
  t.run(20ms, 20ms);
  return t.calls == 2 && t.bus.taken == 1 && t.bus.last().payload[0] == 2;
}

static_assert(test_refused_frame());

// -- a slot set up from the description of its PDO ---------------------------

// A value and a counter of two bits.
struct status {
  std::uint8_t value;
  std::uint8_t counter = 0;
};

// clang-format off
inline constexpr pdo_row<status> status_rows[] = {
{{ 0, 8}, "value", {"Value", "Значение"}, pdo_map<&status::value>},
{{56, 2}, {}, {},                         pdo_map_counter<&status::counter>},
};
// clang-format on

inline constexpr auto status_pdo =
    pdo_of<status_rows>({.name = "status",
                         .title = {"Status", "Состояние"},
                         .direction = pdo_direction::tx,
                         .number = 1,
                         .cob_id = pdo_id::predefined(),
                         .period = 10ms});

// TPDO1 of node 1, set up at 0 ms from the description of its PDO with the
// server's thunk, whose provider puts the number of its calls in the value
// and leaves 3 in the counter, which the slot overwrites; or set up with a
// provider of payloads, which puts AAh in the first byte.
struct described_tpdo1 {
  emb::test::can_bus bus;
  detail::tpdo_producer<1> producer{bus, node_id{1}};
  int calls = 0;
  int plain_calls = 0;

  constexpr described_tpdo1()
  {
    setup_described(0ms);
  }

  constexpr void setup_described(std::chrono::milliseconds now)
  {
    producer.setup<1, pdo_id::predefined()>(
        detail::described_source<status_pdo, &described_tpdo1::provide>(*this),
        status_pdo.pdo.period,
        now);
  }

  constexpr void setup_plain(std::chrono::milliseconds now)
  {
    producer.setup<1, pdo_id::predefined()>(
        {.provider = emb::make_delegate<&described_tpdo1::provide_plain>(this),
         .period = 10ms},
        now);
  }

  constexpr status provide()
  {
    ++calls;
    return {.value = static_cast<std::uint8_t>(calls), .counter = 3};
  }

  constexpr payload_t provide_plain()
  {
    ++plain_calls;
    return {0xAA};
  }

  constexpr void run(std::chrono::milliseconds first,
                     std::chrono::milliseconds last)
  {
    for (auto now = first; now <= last; ++now) {
      producer.tick(now, nmt_state::operational);
    }
  }

  // The value and the counter of the last frame that send took.
  constexpr unsigned value() const
  {
    return bus.last().payload[0];
  }

  constexpr unsigned counter() const
  {
    return bus.last().payload[7];
  }
};

// The slot encodes the message of the provider by the rows of the PDO, on
// the COB-ID of TPDO1, and stamps its own counter of frames over that of the
// provider: 0, 1, 2, 3 and 0 again, in two bits.
consteval bool test_described_counter()
{
  described_tpdo1 t;
  for (unsigned k = 0; k < 5; ++k) {
    t.run(std::chrono::milliseconds{10 * k + 1},
          std::chrono::milliseconds{10 * k + 10});
    if (t.bus.taken != k + 1
        || t.value() != k + 1
        || t.counter() != k % 4
        || t.bus.last().id != cob_id_of<cob_type::tpdo, 1>(node_id{1})) {
      return false;
    }
  }
  if (t.bus.last().payload
      != encode_pdo<status_rows>({.value = 5, .counter = 0})) {
    return false;
  }
  // Setting the slot up again starts its counter at 0.
  t.setup_described(50ms);
  t.run(51ms, 60ms);
  return t.value() == 6 && t.counter() == 0;
}

static_assert(test_described_counter());

// A frame that send refuses does not step the counter: the next frame that it
// takes carries the value that the refused one did, while the provider is
// called once a period, for each frame, refused or not.
consteval bool test_described_refused()
{
  described_tpdo1 t;
  t.run(1ms, 10ms);
  if (t.counter() != 0) return false;
  t.bus.set_queue_full(true);
  t.run(11ms, 30ms);
  if (t.calls != 3 || t.bus.refused != 2) return false;
  t.bus.set_queue_full(false);
  t.run(31ms, 40ms);
  if (t.calls != 4 || t.value() != 4 || t.counter() != 1) return false;
  t.run(41ms, 50ms);
  return t.calls == 5 && t.counter() == 2;
}

static_assert(test_described_refused());

// Each setup writes the slot whole: a provider of payloads in place of a
// source sends its frames alone, and a source in its place again sends its
// own, its counter from 0.
consteval bool test_replaced_source()
{
  described_tpdo1 t;
  t.run(1ms, 20ms);
  if (t.calls != 2 || t.counter() != 1) return false;
  t.setup_plain(20ms);
  t.run(21ms, 40ms);
  if (t.calls != 2 || t.plain_calls != 2 || t.value() != 0xAA) return false;
  t.setup_described(40ms);
  t.run(41ms, 50ms);
  if (t.calls != 3
      || t.plain_calls != 2
      || t.value() != 3
      || t.counter() != 0) {
    return false;
  }
  // A source without a frame in place of a provider leaves nothing of the
  // provider.
  t.setup_plain(50ms);
  t.producer.setup<1, pdo_id::predefined()>(detail::tpdo_source{}, 10ms, 50ms);
  t.run(51ms, 70ms);
  return t.plain_calls == 2 && t.bus.taken == 5;
}

static_assert(test_replaced_source());

// TPDO1 of node 1 whose source has nothing to send every second time it is
// asked, puts the counter it is given, of two bits, in the first byte, and
// counts the frames that it is told send took.
struct sparse_tpdo1 {
  emb::test::can_bus bus;
  detail::tpdo_producer<1> producer{bus, node_id{1}};
  int asked = 0;
  int told = 0;

  constexpr sparse_tpdo1()
  {
    producer.setup<1, pdo_id::predefined()>(
        detail::tpdo_source{
            .next = emb::delegate<std::optional<payload_t>(
                std::uint8_t)>::bind<&sparse_tpdo1::next>(this),
            .sent = emb::delegate<void()>::bind<&sparse_tpdo1::sent>(this),
            .counter_mask = 0x03},
        10ms,
        0ms);
  }

  constexpr std::optional<payload_t> next(std::uint8_t counter)
  {
    ++asked;
    if (asked % 2 == 0) return std::nullopt;
    return payload_t{counter};
  }

  constexpr void sent()
  {
    ++told;
  }

  constexpr void run(std::chrono::milliseconds first,
                     std::chrono::milliseconds last)
  {
    for (auto now = first; now <= last; ++now) {
      producer.tick(now, nmt_state::operational);
    }
  }
};

// A source with nothing to send makes no frame that period, and its counter
// stays; the slot tells it of the frames that send took, and of no other,
// and its counter steps modulo the width of the mask.
consteval bool test_source_without_frame()
{
  sparse_tpdo1 t;
  t.run(1ms, 40ms);
  if (t.asked != 4 || t.bus.taken != 2 || t.told != 2) return false;
  if (t.bus.last().payload[0] != 1) return false;
  t.bus.set_queue_full(true);
  t.run(41ms, 50ms);
  if (t.asked != 5 || t.bus.refused != 1 || t.told != 2) return false;
  t.bus.set_queue_full(false);
  t.run(51ms, 70ms);
  if (t.asked != 7 || t.told != 3 || t.bus.taken != 3) return false;
  if (t.bus.last().payload[0] != 2) return false;
  t.run(71ms, 110ms);
  return t.bus.taken == 5 && t.bus.last().payload[0] == 0;
}

static_assert(test_source_without_frame());

// A PDO without a counter has a mask of 0: the slot stamps nothing, and its
// counter stays 0.
struct plain_status {
  std::uint8_t value;
};

inline constexpr pdo_row<plain_status> plain_status_rows[] = {
    {{0, 8}, "value", {"Value", "Значение"}, pdo_map<&plain_status::value>},
};

static_assert(detail::counter_mask_of<status_rows> == 0x03);
static_assert(detail::counter_mask_of<plain_status_rows> == 0);
static_assert(detail::pdo_counter_row<status_rows> == 1);
static_assert(detail::pdo_counter_row<plain_status_rows> == 1);

// The server takes a provider that returns the message of the PDO, and no
// other: that of another PDO, or one of another type.
struct providers {
  constexpr status provide()
  {
    return {};
  }

  constexpr plain_status provide_other()
  {
    return {};
  }

  constexpr status provide_with(int)
  {
    return {};
  }
};

static_assert(std::same_as<mapped_message_t<status_pdo>, status>);

// The source of a slot set up from the description of a PDO steps its
// counter modulo the width of the counter of the PDO.
consteval bool test_described_mask()
{
  providers p;
  return detail::described_source<status_pdo, &providers::provide>(p)
             .counter_mask
      == 0x03;
}

static_assert(test_described_mask());
static_assert(detail::provides<&providers::provide, providers, status>);
static_assert(!detail::provides<&providers::provide_other, providers, status>);
static_assert(!detail::provides<&providers::provide_with, providers, status>);

} // namespace
