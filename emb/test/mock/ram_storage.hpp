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
  unreadable,
};

// A RAM-backed some_storage for tests. Constexpr throughout, so whole
// scenarios — write, cut the power, reboot, load — run inside static_assert
// and cost nothing at run time.
//
// Parameterized by the three traits that actually differ between media:
//   ram_storage<256>                     — FRAM: byte writes, no erase,
//                                            any range erasable
//   ram_storage<1024, 4, true, 256>      — flash: 4-byte write units,
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
         std::size_t EraseBlock = 1,
         std::byte ErasedValue = std::byte{0xFF}>
class ram_storage {
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

  constexpr ram_storage()
  {
    cells_.fill(ErasedValue);
  }

  constexpr result read(addr_type addr, std::span<std::byte> dest)
  {
    ++read_calls;
    if (read_fault_ || refuses_read(addr, dest.size())) {
      return std::unexpected(storage_fault::unreadable);
    }
    if (!in_range(addr, dest.size())) {
      return std::unexpected(storage_fault::out_of_range);
    }
    std::copy_n(cells_.begin() + addr, dest.size(), dest.begin());
    if (reads_until_corrupt_ == 0) {
      reads_until_corrupt_ = unlimited;
      if (corrupt_offset_ < dest.size()) {
        dest[corrupt_offset_] ^= corrupt_mask_;
      }
    }
    else if (reads_until_corrupt_ != unlimited) {
      --reads_until_corrupt_;
    }
    return {};
  }

  constexpr result write(addr_type addr, std::span<std::byte const> src)
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

    if (write_sink_) return {};

    auto const written = std::min(src.size(), power_budget_);
    std::copy_n(src.begin(), written, cells_.begin() + addr);
    consume(written);
    if (written < src.size()) {
      return std::unexpected(storage_fault::power_loss);
    }
    return {};
  }

  constexpr result erase(addr_type addr, std::size_t len)
  {
    ++erase_calls;
    if (!in_range(addr, len)) {
      return std::unexpected(storage_fault::out_of_range);
    }
    if ((addr % EraseBlock != 0) || (len % EraseBlock != 0)) {
      return std::unexpected(storage_fault::misaligned);
    }

    if (!NeedsErase && write_sink_) return {};

    if (power_budget_ == 0) {
      return std::unexpected(storage_fault::power_loss);
    }
    if (erases_until_cut_ == 0) {
      erases_until_cut_ = unlimited;
      std::fill_n(cells_.begin() + addr,
                  std::min(len, erase_cut_),
                  ErasedValue);
      return std::unexpected(storage_fault::power_loss);
    }
    if (erases_until_cut_ != unlimited) --erases_until_cut_;
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

  // A medium that acknowledges every write and keeps nothing — what a worn
  // out or write-protected FRAM or EEPROM looks like from the outside, and
  // the reason a save reads back what it wrote. On a medium that needs no
  // erase, an erase is a write of `erased_value` over its range, as in
  // `emb::dev::fm25w256::fram`, and is swallowed as well; on one that needs
  // erasing, erases still work.
  constexpr void set_write_sink(bool on)
  {
    write_sink_ = on;
  }

  // A medium that keeps what it is given but cannot be read back: the case
  // where a save's own verification fails although the record landed.
  constexpr void set_read_fault(bool on)
  {
    read_fault_ = on;
  }

  // The medium serves `after` more reads, then refuses the next `count`, then
  // serves again: a read-back that fails while the reads before it worked.
  constexpr void set_read_faults(std::size_t after,
                                 std::size_t count = unlimited)
  {
    reads_until_fault_ = after;
    faulty_reads_ = count;
  }

  // Every read that touches [`addr`, `addr + len`) is refused: one bad spot
  // in an otherwise readable medium.
  constexpr void set_unreadable(addr_type addr, std::size_t len)
  {
    unreadable_begin_ = addr;
    unreadable_end_ = addr + len;
  }

  // After `after` more reads go through intact, the next one to go through
  // returns its bytes with the one at `offset`, counted from the start of
  // that read, XORed with `mask`; the reads after it are intact again, and
  // the cells keep their values. A bit that flips on its way from the
  // medium: a read that differs from what the medium holds, with no error
  // reported.
  constexpr void corrupt_read(std::size_t after,
                              std::size_t offset,
                              std::byte mask)
  {
    reads_until_corrupt_ = after;
    corrupt_offset_ = offset;
    corrupt_mask_ = mask;
  }

  // After `after` more erases go through, the next one loses power: only the
  // first `bytes` bytes of its range reach the erased state, and it reports
  // power_loss. That is what an erase cut short leaves behind.
  constexpr void cut_erase(std::size_t after, std::size_t bytes)
  {
    erases_until_cut_ = after;
    erase_cut_ = bytes;
  }

  constexpr std::span<std::byte> bytes()
  {
    return cells_;
  }

  constexpr std::span<std::byte const> bytes() const
  {
    return cells_;
  }

  std::size_t read_calls = 0;
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

  constexpr bool refuses_read(addr_type addr, std::size_t len)
  {
    if ((addr < unreadable_end_) && (unreadable_begin_ < addr + len)) {
      return true;
    }
    if (reads_until_fault_ > 0) {
      if (reads_until_fault_ != unlimited) --reads_until_fault_;
      return false;
    }
    if (faulty_reads_ == 0) return false;
    if (faulty_reads_ != unlimited) --faulty_reads_;
    return true;
  }

  std::array<std::byte, Capacity> cells_{};
  std::size_t power_budget_ = unlimited;
  bool write_sink_ = false;
  bool read_fault_ = false;
  std::size_t reads_until_fault_ = unlimited;
  std::size_t faulty_reads_ = 0;
  std::size_t unreadable_begin_ = 0;
  std::size_t unreadable_end_ = 0;
  std::size_t erases_until_cut_ = unlimited;
  std::size_t erase_cut_ = 0;
  std::size_t reads_until_corrupt_ = unlimited;
  std::size_t corrupt_offset_ = 0;
  std::byte corrupt_mask_{};
};

} // namespace test
} // namespace emb
