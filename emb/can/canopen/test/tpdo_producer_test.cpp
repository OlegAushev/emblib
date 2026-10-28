#include <emb/can/canopen/detail/tpdo_producer.hpp>
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

} // namespace
