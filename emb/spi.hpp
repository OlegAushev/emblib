#pragma once

#include <emb/meta.hpp>

#include <chrono>
#include <concepts>
#include <expected>
#include <span>

#include <cstddef>
#include <cstdint>

namespace emb {
namespace spi {

// The word the bus shifts. Eight and sixteen bits are what SPI peripherals
// offer; the width is settled where the master is configured, and a driver
// that can only work in one of them constrains on it.
template<typename T>
concept some_frame = emb::same_as_any<T, std::uint8_t, std::uint16_t>;

// How long a part needs chip select settled around the clocked data — tCSS
// and tCSH in its datasheet.
//
// The values belong to the part, so the part's driver supplies them;
// honouring them needs a clock, which only the master has. Both are lower
// bounds: a master whose own overhead between asserting CS and the first
// clock edge already exceeds `setup` has nothing left to wait for.
struct cs_timing {
  std::chrono::nanoseconds setup{};
  std::chrono::nanoseconds hold{};
};

// One part's view of the bus it hangs on: shift frames while selected, and
// nothing beyond that.
//
// Deliberately not a master. Which line selects the part, how fast the
// clock runs, in which mode, and how long a transfer may take before it is
// abandoned are facts about the board and the peripheral — they are settled
// where the master is configured. A driver that had to name its own slave
// index could not be written once and mounted twice.
//
// Transfers are spans, never single frames. A master that can move a block
// at a time (DMA, a FIFO) must be able to say so, and a per-frame interface
// would hide that behind a loop forever; a driver that really wants one
// frame passes a span of one. `read` clocks out whatever the master sends
// while idle — the part being read ignores MOSI — and `transfer` is the
// full-duplex form, where the two spans must be the same length.
//
// The error type belongs to the master, for the reason emb/nvm/storage.hpp
// gives about storage backends: a bus reports its own failures, and
// collapsing them into a common enum here would discard detail at the wrong
// end. A driver that owes its caller one error type of its own carries the
// master's inside it rather than flattening it away.
//
// assert_cs() and deassert_cs() are the raw edges of chip select. They are
// spelled that way rather than select/release because an implementation may
// already own those words for a bare pin movement, while these two also owe
// whatever the bus needs around it — settling time, a drained receive
// buffer — and the two contracts must not answer to one name.
//
// Drivers do not call them: they declare a `selection` (below), which
// cannot leak an asserted CS down an early-return path.
template<typename T>
concept some_device =
    requires {
      typename T::frame_type;
      typename T::error_type;
      requires some_frame<typename T::frame_type>;
    }
    && requires(T& dev,
                cs_timing timing,
                std::span<typename T::frame_type const> tx,
                std::span<typename T::frame_type> rx) {
         { dev.assert_cs(timing) } -> std::same_as<void>;
         { dev.deassert_cs(timing) } -> std::same_as<void>;
         {
           dev.write(tx)
         } -> std::same_as<std::expected<void, typename T::error_type>>;
         {
           dev.read(rx)
         } -> std::same_as<std::expected<void, typename T::error_type>>;
         {
           dev.transfer(tx, rx)
         } -> std::same_as<std::expected<void, typename T::error_type>>;
       };

// Result of a device operation, spelled once for the code that chains them.
//
// Constrained on the error type alone rather than on some_device, so that a
// device may name it in its own signatures: inside its class body the type
// is still incomplete, and a some_device check there could never be
// satisfied — it is what the concept is about to verify.
template<typename Dev>
  requires requires { typename Dev::error_type; }
using result = std::expected<void, typename Dev::error_type>;

// Chip select held for a scope:
//
//   emb::spi::selection const cs{dev_, cs_timing};
//   TRY(dev_.write(header));
//   return dev_.read(buf);
//
// CS is released on every exit path, including the early return TRY hides.
// A transaction taking a callback releases just as reliably, but every step
// then nests one lambda deeper; this stays flat.
//
// Neither copyable nor movable: a selection names a scope, and there is
// nowhere for one to travel to.
template<some_device Dev>
class [[nodiscard]] selection {
public:
  selection(Dev& dev, cs_timing timing) : dev_(dev), timing_(timing)
  {
    dev_.assert_cs(timing_);
  }

  selection(selection const&) = delete;
  selection& operator=(selection const&) = delete;
  selection(selection&&) = delete;
  selection& operator=(selection&&) = delete;

  ~selection()
  {
    dev_.deassert_cs(timing_);
  }

private:
  Dev& dev_;
  cs_timing timing_;
};

// The byte view of an eight-bit bus. Most parts are byte-oriented and their
// callers hold std::byte — emb::nvm speaks it — while a master shifts
// unsigned words. This is the one place the two spellings meet, so that no
// driver has to cast per frame.
template<some_device Dev>
  requires std::same_as<typename Dev::frame_type, std::uint8_t>
auto write_bytes(Dev& dev, std::span<std::byte const> src) -> result<Dev>
{
  return dev.write(
      std::span{reinterpret_cast<std::uint8_t const*>(src.data()), src.size()});
}

template<some_device Dev>
  requires std::same_as<typename Dev::frame_type, std::uint8_t>
auto read_bytes(Dev& dev, std::span<std::byte> dest) -> result<Dev>
{
  return dev.read(
      std::span{reinterpret_cast<std::uint8_t*>(dest.data()), dest.size()});
}

template<some_device Dev>
  requires std::same_as<typename Dev::frame_type, std::uint8_t>
auto transfer_bytes(Dev& dev,
                    std::span<std::byte const> src,
                    std::span<std::byte> dest) -> result<Dev>
{
  return dev.transfer(
      std::span{reinterpret_cast<std::uint8_t const*>(src.data()), src.size()},
      std::span{reinterpret_cast<std::uint8_t*>(dest.data()), dest.size()});
}

} // namespace spi
} // namespace emb
