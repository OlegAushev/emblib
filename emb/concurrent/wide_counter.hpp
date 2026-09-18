#pragma once

#include <atomic>
#include <cstdint>

namespace emb {

// 63-bit counter that one context increments and any context reads,
// with no retry loop and no masked interrupts, on one core or several.
//
// lo_ holds the low 32 bits. hi_ holds the number of lo_ wraps in bits
// 0..30 and, in bit 31, a copy of lo_'s bit 31 as of the last hi_ store
// (the scheme of Linux's cnt32_to_63). increment() stores hi_ with release
// after lo_, and load() loads hi_ with acquire before lo_, so the hi_ a
// reader gets is never newer than the lo_ it gets: at most one half-period
// of lo_ behind. When it is behind, the copied bit differs from lo_'s
// bit 31 and the reader takes the missing step itself. Dropping the release
// or the acquire, or loading lo_ first, breaks this.
//
// Unlike seqlock, a reader may preempt the writer anywhere, so load() is
// safe at any priority, NMI included.
// Constraints:
//   - exactly one context calls increment()
//   - a reader must not stall between its two loads for 2^31 increments
//     (24.8 days at 1 kHz)
//   - load() orders only the counter, not what the writer stored before
//     increment()
//   - the count stays valid up to 2^63 - 1
class wide_counter {
private:
  static constexpr std::uint32_t msb = 0x8000'0000u;

  std::atomic<std::uint32_t> lo_ = 0;
  std::atomic<std::uint32_t> hi_ = 0;
public:
  void increment()
  {
    std::uint32_t const lo = lo_.load(std::memory_order::relaxed) + 1;
    lo_.store(lo, std::memory_order::relaxed);
    std::uint32_t const hi = hi_.load(std::memory_order::relaxed);
    if (lags(hi, lo)) {
      hi_.store(advance(hi), std::memory_order::release);
    }
  }

  std::uint64_t load() const
  {
    std::uint32_t hi = hi_.load(std::memory_order::acquire);
    std::uint32_t const lo = lo_.load(std::memory_order::relaxed);
    if (lags(hi, lo)) {
      hi = advance(hi);
    }
    return (std::uint64_t{hi & ~msb} << 32) | lo;
  }
private:
  static constexpr bool lags(std::uint32_t hi, std::uint32_t lo)
  {
    return ((hi ^ lo) & msb) != 0;
  }

  // Flips the copied bit; when it falls from 1 to 0, lo_ has wrapped
  // and the wrap count goes up.
  static constexpr std::uint32_t advance(std::uint32_t hi)
  {
    return (hi ^ msb) + (hi >> 31);
  }
};

} // namespace emb
