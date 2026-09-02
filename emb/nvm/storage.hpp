#pragma once

#include <algorithm>
#include <concepts>
#include <expected>
#include <span>

#include <cstddef>

namespace emb {
namespace nvm {

// A byte-addressed non-volatile memory seen as blocks: what FRAM, EEPROM and
// internal flash have in common, and nothing beyond it.
//
// The vocabulary is erase-aware even where erasing has no hardware meaning:
// `erase` means "bring this range to erased_value" — a sector erase on
// flash, a plain overwrite on FRAM — while `needs_erase` says whether
// `write` *requires* its target to be in that state beforehand. Code that
// honours both works unchanged on all three media, and an explicit wipe of
// stored data stays honest on a memory that has no erased state.
//
// `write` is named for what the caller does, not for what the medium
// does: it is the inverse of `read` only where needs_erase is false. On
// flash it can merely take bits out of the erased state, so overwriting
// in place is never a thing to assume from the name — consult
// needs_erase.
//
// The error type belongs to the backend: drivers already have their own
// vocabulary (emb::nvm::error for the FRAM driver, an HAL status for
// internal flash), and imposing a common enum here would only add a mapping
// layer at the wrong end. Callers that need one error type map at their own
// boundary, where the backend is known.
//
// Erase block geometry is deliberately absent: flash sectors are not
// uniform (F4 has 16K/64K/128K sectors in one bank), so no single constant
// describes them. Where a region must align to erase blocks, the truth
// lives in the layout the application declares, and the backend rejects a
// range that does not cover whole blocks.
//
// Preconditions a backend may assume, and a caller must respect:
//  - every range lies inside [0, capacity);
//  - write addresses and lengths are multiples of write_granularity;
//  - an erase range covers whole erase blocks of the medium.
template<typename T>
concept some_block_storage =
    requires {
      typename T::addr_type;
      typename T::error_type;
      { T::capacity } -> std::convertible_to<std::size_t>;
      { T::write_granularity } -> std::convertible_to<std::size_t>;
      { T::needs_erase } -> std::convertible_to<bool>;
      { T::erased_value } -> std::convertible_to<std::byte>;
    }
    && requires(T& storage,
                typename T::addr_type addr,
                std::span<std::byte> dest,
                std::span<std::byte const> src,
                std::size_t len) {
         {
           storage.read(addr, dest)
         } -> std::same_as<std::expected<void, typename T::error_type>>;
         {
           storage.write(addr, src)
         } -> std::same_as<std::expected<void, typename T::error_type>>;
         {
           storage.erase(addr, len)
         } -> std::same_as<std::expected<void, typename T::error_type>>;
       };

// Result of a backend operation, spelled once for the code that chains them.
template<some_block_storage Storage>
using result = std::expected<void, typename Storage::error_type>;

// Whether a range read back from storage is still in the erased state — how
// a slot scan tells "never written" from "written and then corrupted".
template<some_block_storage Storage>
constexpr bool is_erased(std::span<std::byte const> data)
{
  return std::ranges::all_of(data, [](std::byte b) {
    return b == Storage::erased_value;
  });
}

} // namespace nvm
} // namespace emb
