#pragma once

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>

#include <emb/can.hpp>
#include <emb/can/bus.hpp>
#include <emb/delegate.hpp>

#include "../types.hpp"

namespace emb {
namespace can {
namespace canopen {
namespace detail {

// The struct `tpdo_source` is what a TPDO slot takes its frames from, as the
// server sets up the slot of a PDO from its description: `next` returns the
// payload to send with the value of the slot's counter of frames, or none if
// it has nothing to send this period; `sent`, if set, learns that `send` took
// the payload of the last `next`; and `counter_mask`, the mask of the width
// of the counter, by which the slot steps it, is 0 for a PDO without one.
struct tpdo_source {
  emb::delegate<std::optional<payload_t>(std::uint8_t)> next;
  emb::delegate<void()> sent;
  std::uint8_t counter_mask = 0;
};

template<std::size_t N>
class tpdo_producer {
public:
  constexpr tpdo_producer(transport& bus, node_id node) : bus_(bus), node_(node)
  {
  }

  // Sets slot `I` up to send the payload of the provider of `cfg` once its
  // period. The slot is written whole: a source it had goes.
  template<std::size_t I, pdo_id CobId>
  constexpr void setup(tpdo_config const& cfg, std::chrono::milliseconds now)
  {
    static_assert(I >= 1 && I <= N, "TPDO index out of range");
    slots_[I - 1] = {.cob_id = cob_id<I, CobId>(),
                     .provider = cfg.provider,
                     .period = cfg.period,
                     .last_tx = now};
  }

  // Sets slot `I` up to send what `source` gives once `period`, its counter
  // of frames starting at 0. The slot is written whole: a provider it had
  // goes.
  template<std::size_t I, pdo_id CobId>
  constexpr void setup(tpdo_source const& source,
                       std::chrono::milliseconds period,
                       std::chrono::milliseconds now)
  {
    static_assert(I >= 1 && I <= N, "TPDO index out of range");
    slots_[I - 1] = {.cob_id = cob_id<I, CobId>(),
                     .next = source.next,
                     .sent = source.sent,
                     .counter_mask = source.counter_mask,
                     .period = period,
                     .last_tx = now};
  }

  // If the node is operational, sends a frame from each slot whose period has
  // passed since its last frame: the payload of its provider, or what its
  // source gives for the value of its counter. A slot calls its provider or
  // its source at most once a period: the period runs from the last frame
  // whether or not `send` took it, so a frame that `send` refused, the
  // transmit queue being full, is not sent again, and the next period sends
  // a fresh one. Only when `send` takes the frame of a source does the slot
  // tell the source and step its counter, modulo the width of the counter:
  // the counter counts the frames that went into the transmit queue, and the
  // next frame after a refused one carries the value that the refused one
  // did. A source with nothing to send this period sends no frame.
  constexpr void tick(std::chrono::milliseconds now, nmt_state state)
  {
    if (state != nmt_state::operational) return;

    for (auto& s : slots_) {
      if (!s.provider && !s.next) continue;
      if (s.period == std::chrono::milliseconds::zero()) continue;
      if ((now - s.last_tx) < s.period) continue;

      s.last_tx = now;
      auto const payload =
          s.next ? s.next(s.counter) : std::optional<payload_t>{s.provider()};
      if (!payload) continue;
      frame_t const frame = {.format = format_t::standard,
                             .id = s.cob_id,
                             .len = 8,
                             .payload = *payload};
      if (!bus_.send(frame)) continue;
      if (s.sent) s.sent();
      s.counter = static_cast<std::uint8_t>((s.counter + 1u) & s.counter_mask);
    }
  }

private:
  // A slot sends the frames of a provider of payloads, as a `tpdo_config`
  // gives it, or of a source, `next`, `sent` and the counter: one of them,
  // which `setup` sets, the others empty. On a 32-bit target it takes 48
  // bytes.
  struct slot {
    id_t cob_id = 0;
    emb::delegate<payload_t()> provider{};
    emb::delegate<std::optional<payload_t>(std::uint8_t)> next{};
    emb::delegate<void()> sent{};
    std::uint8_t counter = 0;
    std::uint8_t counter_mask = 0;
    std::chrono::milliseconds period{0};
    std::chrono::milliseconds last_tx{0};
  };

  // Returns the COB-ID of slot `I`: `CobId` if it is custom, else the
  // predefined COB-ID of TPDO `I` of the node.
  template<std::size_t I, pdo_id CobId>
  constexpr id_t cob_id() const
  {
    if constexpr (CobId.is_custom) {
      return CobId.value;
    }
    else {
      return cob_id_of<cob_type::tpdo, I>(node_);
    }
  }

  transport& bus_;
  node_id node_;
  std::array<slot, N> slots_;
};

} // namespace detail
} // namespace canopen
} // namespace can
} // namespace emb
