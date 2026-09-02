#include <array>
#include <expected>
#include <span>

#include <emb/nvm/storage.hpp>
#include <emb/test/mock/block_storage.hpp>

namespace {

using namespace emb;
using test::storage_fault;

// FRAM: byte-granular writes, no erase needed.
using fram_storage = test::block_storage<64>;

// Internal flash: 4-byte write units, target must be erased, 32-byte
// erase blocks (a scaled-down stand-in for a sector).
using flash_storage = test::block_storage<128, 4, true, 32>;

// -- Concept --

static_assert(nvm::some_block_storage<fram_storage>);
static_assert(nvm::some_block_storage<flash_storage>);

struct no_erase {
  using addr_type = std::uint32_t;
  using error_type = int;
  static constexpr std::size_t capacity = 8;
  static constexpr std::size_t write_granularity = 1;
  static constexpr bool needs_erase = false;
  static constexpr std::byte erased_value{0xFF};

  auto read(addr_type, std::span<std::byte>) -> std::expected<void, int>;
  auto write(addr_type, std::span<std::byte const>)
      -> std::expected<void, int>;
};

struct no_error_type {
  using addr_type = std::uint32_t;
  static constexpr std::size_t capacity = 8;
  static constexpr std::size_t write_granularity = 1;
  static constexpr bool needs_erase = false;
  static constexpr std::byte erased_value{0xFF};

  auto read(addr_type, std::span<std::byte>) -> std::expected<void, int>;
  auto write(addr_type, std::span<std::byte const>)
      -> std::expected<void, int>;
  auto erase(addr_type, std::size_t) -> std::expected<void, int>;
};

// Reports success as a bool: silently loses the reason a write failed,
// which is the whole point of the expected-returning contract.
struct bool_returning {
  using addr_type = std::uint32_t;
  using error_type = int;
  static constexpr std::size_t capacity = 8;
  static constexpr std::size_t write_granularity = 1;
  static constexpr bool needs_erase = false;
  static constexpr std::byte erased_value{0xFF};

  auto read(addr_type, std::span<std::byte>) -> std::expected<void, int>;
  bool write(addr_type, std::span<std::byte const>);
  auto erase(addr_type, std::size_t) -> std::expected<void, int>;
};

struct untyped_erased_value {
  using addr_type = std::uint32_t;
  using error_type = int;
  static constexpr std::size_t capacity = 8;
  static constexpr std::size_t write_granularity = 1;
  static constexpr bool needs_erase = false;
  static constexpr int erased_value = 0xFF;

  auto read(addr_type, std::span<std::byte>) -> std::expected<void, int>;
  auto write(addr_type, std::span<std::byte const>)
      -> std::expected<void, int>;
  auto erase(addr_type, std::size_t) -> std::expected<void, int>;
};

static_assert(!nvm::some_block_storage<no_erase>);
static_assert(!nvm::some_block_storage<no_error_type>);
static_assert(!nvm::some_block_storage<bool_returning>);
static_assert(!nvm::some_block_storage<untyped_erased_value>);

// -- Behaviour --
//
// Checks report failure by returning false rather than by assert(): under
// NDEBUG an assert in a constexpr test evaluates to nothing and the case
// passes silently.

consteval bool test_round_trip()
{
  fram_storage s;
  std::array const data{std::byte{0x11}, std::byte{0x22}, std::byte{0x33}};

  if (!s.write(4, data)) return false;

  std::array<std::byte, 3> buf{};
  if (!s.read(4, buf)) return false;
  if (buf != data) return false;

  // The byte before the written range is untouched.
  std::array<std::byte, 1> neighbour{};
  if (!s.read(3, neighbour)) return false;
  if (neighbour[0] != fram_storage::erased_value) return false;

  return true;
}

consteval bool test_erase_wipes_a_memory_without_an_erased_state()
{
  fram_storage s;
  std::array const data{std::byte{0x5A}, std::byte{0xA5}};

  if (!s.write(0, data)) return false;
  if (nvm::is_erased<fram_storage>(s.bytes())) return false;

  if (!s.erase(0, fram_storage::capacity)) return false;
  if (!nvm::is_erased<fram_storage>(s.bytes())) return false;

  return true;
}

consteval bool test_write_needs_an_erased_target()
{
  flash_storage s;
  std::array const word{std::byte{1}, std::byte{2}, std::byte{3},
                        std::byte{4}};

  if (!s.write(0, word)) return false;

  auto const again = s.write(0, word);
  if (again) return false;
  if (again.error() != storage_fault::not_erased) return false;

  if (!s.erase(0, flash_storage::erase_block)) return false;
  if (!s.write(0, word)) return false;

  return true;
}

consteval bool test_alignment()
{
  flash_storage s;
  std::array const word{std::byte{1}, std::byte{2}, std::byte{3},
                        std::byte{4}};
  std::array const partial{std::byte{1}, std::byte{2}, std::byte{3}};

  auto const off_address = s.write(1, word);
  if (off_address || off_address.error() != storage_fault::misaligned) {
    return false;
  }

  auto const off_length = s.write(0, partial);
  if (off_length || off_length.error() != storage_fault::misaligned) {
    return false;
  }

  auto const partial_block = s.erase(0, flash_storage::erase_block / 2);
  if (partial_block || partial_block.error() != storage_fault::misaligned) {
    return false;
  }

  auto const off_block = s.erase(4, flash_storage::erase_block);
  if (off_block || off_block.error() != storage_fault::misaligned) {
    return false;
  }

  return true;
}

consteval bool test_power_loss_leaves_the_written_prefix()
{
  flash_storage s;
  std::array const data{std::byte{1}, std::byte{2}, std::byte{3},
                        std::byte{4}, std::byte{5}, std::byte{6},
                        std::byte{7}, std::byte{8}};

  s.set_power_budget(4);

  auto const cut = s.write(0, data);
  if (cut) return false;
  if (cut.error() != storage_fault::power_loss) return false;

  std::array<std::byte, 8> buf{};
  if (!s.read(0, buf)) return false;
  for (auto i = 0uz; i < 4; ++i)
    if (buf[i] != data[i]) return false;
  for (auto i = 4uz; i < 8; ++i)
    if (buf[i] != flash_storage::erased_value) return false;

  // The medium stays dead: nothing written after the cut.
  auto const after = s.write(8, std::span{data}.first(4));
  if (after) return false;
  if (after.error() != storage_fault::power_loss) return false;

  std::array<std::byte, 4> tail{};
  if (!s.read(8, tail)) return false;
  if (!nvm::is_erased<flash_storage>(tail)) return false;

  return true;
}

consteval bool test_bounds()
{
  fram_storage s;
  std::array<std::byte, 4> buf{};

  auto const past_end = s.read(fram_storage::capacity - 2, buf);
  if (past_end || past_end.error() != storage_fault::out_of_range) {
    return false;
  }

  auto const at_end = s.write(fram_storage::capacity, buf);
  if (at_end || at_end.error() != storage_fault::out_of_range) return false;

  auto const too_long = s.erase(0, fram_storage::capacity + 1);
  if (too_long || too_long.error() != storage_fault::out_of_range) {
    return false;
  }

  // An empty range at the very end is still in bounds.
  if (!s.read(fram_storage::capacity, std::span<std::byte>{})) return false;

  return true;
}

static_assert(test_round_trip());
static_assert(test_erase_wipes_a_memory_without_an_erased_state());
static_assert(test_write_needs_an_erased_target());
static_assert(test_alignment());
static_assert(test_power_loss_leaves_the_written_prefix());
static_assert(test_bounds());

} // namespace
