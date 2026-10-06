#pragma once

#include <algorithm>
#include <array>
#include <bitset>
#include <chrono>
#include <concepts>
#include <cstddef>
#include <cstdint>

namespace emb {
namespace can {
namespace canopen {

// Structure holding how long a lost link stays at each severity: it is a
// warning from the moment a watched frame misses its timeout, an error once
// it has stayed lost for `to_error`, and critical once it has stayed lost for
// `to_critical`. `never` keeps a link from reaching that severity.
struct lost_timing {
  std::chrono::milliseconds to_error;
  std::chrono::milliseconds to_critical;
};

inline constexpr auto never = std::chrono::milliseconds::max();

// Severity of a lost link, as `node_link::loss` reports it.
enum class link_loss : std::uint8_t {
  none,
  warning,
  error,
  critical
};

// Tag telling a link to watch each frame from its first valid arrival rather
// than from the start.
struct from_first_frame_t {
  explicit from_first_frame_t() = default;
};

inline constexpr from_first_frame_t from_first_frame{};

// The class template `node_link` supervises the PDOs a remote node sends:
// `FrameCount` frames, each with its own timeout and a rolling counter of
// `CounterBits` bits. The frame handler asks `counter_ok` whether a frame
// follows the one before it, checks whatever else makes the frame valid, and
// reports a valid frame with `report_valid`. Only a valid frame keeps its
// stream alive; an invalid one is not reported to the link at all, and how it
// shows is up to the caller. `loss` tells how badly the link is lost, from the
// earliest deadline among the watched frames and `lost_timing`. The link
// raises nothing itself: the caller turns its answer into whatever it reports.
//
// Every member takes the time it is called at, as the caller's clock counts
// it, and all of them must be called from one context.
template<std::size_t FrameCount, unsigned CounterBits = 2>
class node_link {
  static_assert(FrameCount > 0, "a link watches at least one frame");
  static_assert(CounterBits > 0 && CounterBits <= 8,
                "a counter takes one to eight bits");

public:
  using timeout_table = std::array<std::chrono::milliseconds, FrameCount>;

  constexpr node_link(lost_timing timing,
                      timeout_table timeouts,
                      std::chrono::milliseconds now)
      : timing_(timing), timeouts_(timeouts)
  {
    last_good_.fill(now);
    watched_.set();
  }

  // A producer that may never show up — a test bench streaming substitutes —
  // is not reported lost before it has been heard: each of its frames is
  // watched from its first valid arrival, and a frame nobody sends is never
  // missed. Once heard, a frame is watched like any other, and falling silent
  // again is a loss, not an end: only a restart stops the watch.
  constexpr node_link(from_first_frame_t,
                      lost_timing timing,
                      timeout_table timeouts)
      : timing_(timing), timeouts_(timeouts)
  {
  }

  template<std::size_t Frame>
    requires(Frame >= 1 && Frame <= FrameCount)
  constexpr bool counter_ok(std::unsigned_integral auto counter,
                            std::chrono::milliseconds now)
  {
    return counter_ok_at(Frame - 1,
                         static_cast<std::uint8_t>(counter & counter_mask),
                         now);
  }

  template<std::size_t Frame>
    requires(Frame >= 1 && Frame <= FrameCount)
  constexpr void report_valid(std::chrono::milliseconds now)
  {
    last_good_[Frame - 1] = now;
    watched_.set(Frame - 1);
  }

  constexpr link_loss loss(std::chrono::milliseconds now) const
  {
    auto const deadline = connected_until();
    if (now < deadline) {
      return link_loss::none;
    }
    auto const down_for = now - deadline;
    if (down_for >= timing_.to_critical) {
      return link_loss::critical;
    }
    if (down_for >= timing_.to_error) {
      return link_loss::error;
    }
    return link_loss::warning;
  }

private:
  static constexpr std::uint8_t counter_mask =
      static_cast<std::uint8_t>((1u << CounterBits) - 1);

  constexpr bool counter_ok_at(std::size_t i,
                               std::uint8_t seen,
                               std::chrono::milliseconds now)
  {
    auto const expected =
        static_cast<std::uint8_t>((prev_counter_[i] + 1) & counter_mask);
    // A frame that arrives after a gap in its stream opens a new sequence:
    // whatever it carries cannot be compared with a counter from before the
    // gap, and a recovery must not be charged for the frames it missed. The
    // gap runs from the last frame received, valid or not. Measured from the
    // last valid one, a stream whose counter has stopped would have one frame
    // a timeout taken for a new sequence, and the link would never be lost.
    // The first frame of a stream not watched yet opens a sequence the same
    // way, whatever arrived before it.
    bool const resumed = !watched_.test(i)
                      || !counter_seen_.test(i)
                      || (now - last_seen_[i]) >= timeouts_[i];
    prev_counter_[i] = seen;
    last_seen_[i] = now;
    counter_seen_.set(i);
    return resumed || seen == expected;
  }

  // A frame not watched yet sets no deadline; with none watched at all the
  // link counts as connected.
  constexpr std::chrono::milliseconds connected_until() const
  {
    auto earliest = std::chrono::milliseconds::max();
    for (auto i = 0uz; i < FrameCount; ++i) {
      if (watched_.test(i)) {
        earliest = std::min(earliest, last_good_[i] + timeouts_[i]);
      }
    }
    return earliest;
  }

  lost_timing timing_;
  timeout_table timeouts_;

  std::array<std::chrono::milliseconds, FrameCount> last_good_{};
  std::array<std::chrono::milliseconds, FrameCount> last_seen_{};
  std::bitset<FrameCount> watched_;
  std::bitset<FrameCount> counter_seen_;
  std::array<std::uint8_t, FrameCount> prev_counter_{};
};

} // namespace canopen
} // namespace can
} // namespace emb
