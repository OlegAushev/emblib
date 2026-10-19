#pragma once

#include <cstdint>

namespace emb {
namespace can {

// The class template `rolling_counter` numbers the frames of one stream a node
// sends, with a counter of `Bits` bits that wraps to zero after its largest
// value: the counter that the receiver's `node_link` of the same width checks.
// `stamp` writes the number of the next frame into the frame's `counter` field
// and steps to the one after it; the first frame carries zero.
//
// The number goes into the frame rather than out of a getter: masked where it
// is assigned, it fits a bit-field as narrow as the counter without a
// -Wconversion warning, which an `std::uint8_t` from a getter would raise.
template<unsigned Bits>
class rolling_counter {
  static_assert(Bits > 0 && Bits <= 8, "a counter takes one to eight bits");

public:
  // The counter's bits, all set: its largest value.
  static constexpr std::uint8_t mask =
      static_cast<std::uint8_t>((1u << Bits) - 1);

  template<typename Frame>
  constexpr void stamp(Frame& frame)
  {
    frame.counter = next_ & mask;
    next_ = (next_ + 1) & mask;
  }

private:
  std::uint8_t next_ = 0;
};

} // namespace can
} // namespace emb
