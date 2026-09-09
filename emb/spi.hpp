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

// The word the bus shifts.
template<typename T>
concept some_frame = emb::same_as_any<T, std::uint8_t, std::uint16_t>;

// How long a part needs chip select settled around the clocked data — tCSS
// and tCSH in its datasheet. Both are lower bounds.
struct cs_timing {
  std::chrono::nanoseconds setup{};
  std::chrono::nanoseconds hold{};
};

// One part's view of the bus it hangs on: shift frames while selected.
//
// `read` clocks out whatever the master sends while idle; `transfer` is the
// full-duplex form, and its two spans must be the same length. The error
// type is the master's. Chip select is taken through `selection` below,
// not by calling assert_cs and deassert_cs.
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

// Result of a device operation. Constrained on the error type alone: a
// device names this in its own signatures, where the type is still
// incomplete and a some_device check could never be satisfied.
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

// The byte view of an eight-bit bus: emb::nvm speaks std::byte, a
// peripheral shifts unsigned words.
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
