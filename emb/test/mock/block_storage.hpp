#pragma once

#include <emb/nvm/storage.hpp>

#include <algorithm>
#include <array>
#include <expected>
#include <span>

#include <cstddef>
#include <cstdint>

namespace emb {
namespace test {

enum class storage_fault {
  out_of_range,
  misaligned,
  not_erased,
  power_loss,
};

// A RAM-backed some_block_storage for tests. Constexpr throughout, so whole
// scenarios — write, cut the power, reboot, load — run inside static_assert
// and cost nothing at run time.
//
// Parameterized by the three traits that actually differ between media:
//   block_storage<256>                     — FRAM: byte writes, no erase
//   block_storage<1024, 4, true, 256>      — flash: 4-byte write units,
//                                            erased target required,
//                                            256-byte erase blocks
//
// Deliberately stricter than real NOR flash: writing a byte that is not
// in the erased state is an error here, where hardware would silently AND
// the bits together. A store that forgets an erase must fail the test
// rather than pass by luck.
template<std::size_t Capacity,
         std::size_t WriteGranularity = 1,
         bool NeedsErase = false,
         std::size_t EraseBlock = Capacity,
         std::byte ErasedValue = std::byte{0xFF}>
class block_storage {
public:
  using addr_type = std::uint32_t;
  using error_type = storage_fault;

  static constexpr std::size_t capacity = Capacity;
  static constexpr std::size_t write_granularity = WriteGranularity;
  static constexpr bool needs_erase = NeedsErase;
  static constexpr std::size_t erase_block = EraseBlock;
  static constexpr std::byte erased_value = ErasedValue;

  static constexpr std::size_t unlimited = SIZE_MAX;

  using result = std::expected<void, error_type>;

  constexpr block_storage()
  {
    cells_.fill(ErasedValue);
  }

  constexpr auto read(addr_type addr, std::span<std::byte> dest) -> result
  {
    if (!in_range(addr, dest.size())) {
      return std::unexpected(storage_fault::out_of_range);
    }
    std::copy_n(cells_.begin() + addr, dest.size(), dest.begin());
    return {};
  }

  constexpr auto write(addr_type addr, std::span<std::byte const> src)
      -> result
  {
    ++write_calls;
    if (!in_range(addr, src.size())) {
      return std::unexpected(storage_fault::out_of_range);
    }
    if ((addr % WriteGranularity != 0)
        || (src.size() % WriteGranularity != 0)) {
      return std::unexpected(storage_fault::misaligned);
    }
    if constexpr (NeedsErase) {
      for (auto i = 0uz; i < src.size(); ++i)
        if (cells_[addr + i] != ErasedValue) {
          return std::unexpected(storage_fault::not_erased);
        }
    }

    auto const written = std::min(src.size(), power_budget_);
    std::copy_n(src.begin(), written, cells_.begin() + addr);
    consume(written);
    if (written < src.size()) {
      return std::unexpected(storage_fault::power_loss);
    }
    return {};
  }

  constexpr auto erase(addr_type addr, std::size_t len) -> result
  {
    ++erase_calls;
    if (!in_range(addr, len)) {
      return std::unexpected(storage_fault::out_of_range);
    }
    if ((addr % EraseBlock != 0) || (len % EraseBlock != 0)) {
      return std::unexpected(storage_fault::misaligned);
    }
    if (power_budget_ == 0) {
      return std::unexpected(storage_fault::power_loss);
    }
    std::fill_n(cells_.begin() + addr, len, ErasedValue);
    return {};
  }

  // -- Test controls --

  // Power-loss injection: the medium accepts this many more bytes in
  // total, then the call that runs out writes its prefix and reports
  // power_loss; every later write leaves the memory untouched. That is
  // exactly what an interrupted commit leaves behind.
  constexpr void set_power_budget(std::size_t bytes)
  {
    power_budget_ = bytes;
  }

  constexpr auto bytes() -> std::span<std::byte>
  {
    return cells_;
  }

  constexpr auto bytes() const -> std::span<std::byte const>
  {
    return cells_;
  }

  std::size_t write_calls = 0;
  std::size_t erase_calls = 0;

private:
  constexpr bool in_range(addr_type addr, std::size_t len) const
  {
    return (addr <= Capacity) && (len <= Capacity - addr);
  }

  constexpr void consume(std::size_t bytes)
  {
    if (power_budget_ != unlimited) power_budget_ -= bytes;
  }

  std::array<std::byte, Capacity> cells_{};
  std::size_t power_budget_ = unlimited;
};

} // namespace test
} // namespace emb
