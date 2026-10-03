#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include <emb/nvm/storage.hpp>
#include <emb/settings/store.hpp>
#include <emb/test/mock/ram_storage.hpp>

namespace {

using namespace emb;
using namespace emb::settings;

using emb::test::storage_fault;

inline constexpr std::uint32_t magic = 0x52414554u; // "TEAR"

// One parameter: a record of 32 bytes, 24 of body and 8 of footer.
inline constexpr auto schema =
    make_schema(param("generation", std::uint32_t{0}));

inline constexpr auto body_bytes = record_body_size(schema.count);
inline constexpr auto record_bytes = record_size(schema.count);

// FRAM: byte writes, no erase, two slots of one record each.
using fram = test::ram_storage<64>;
inline constexpr placement fram_placement{.magic = magic,
                                          .base = 0,
                                          .slot_capacity = record_bytes,
                                          .slot_count = 2,
                                          .slots_per_block = 1};

// Internal flash: four-byte writes, an erased target required, two erase
// blocks of two slots each.
using flash = test::ram_storage<128, 4, true, 64>;
inline constexpr placement flash_placement{.magic = magic,
                                           .base = 0,
                                           .slot_capacity = record_bytes,
                                           .slot_count = 4,
                                           .slots_per_block = 2};

consteval image<schema> generation(std::size_t n)
{
  image<schema> values;
  auto _ = values.set<"generation">(static_cast<std::uint32_t>(n));
  return values;
}

// Cuts a save after each number of bytes it can write, from none to all of
// them, with `phase` saves gone through before it, and checks what a restart
// finds: the record the save makes if that record is whole on the medium,
// i.e. if the save went through or the bytes it did not write already held
// their values, and otherwise the newest record before it. Either way the
// save left the slot of that newest record as it was, the next save goes
// through, and the restart after it loads that one.
template<typename Memory, placement Placement>
consteval bool power_lost_at_every_byte(std::size_t phase)
{
  using store_type = store<schema, Memory, Placement>;

  Memory before;
  std::optional<save_result> newest;
  for (auto n = 1uz; n <= phase; ++n) {
    auto const saved = store_type{before}.save(generation(n));
    if (!saved) return false;
    newest = *saved;
  }

  auto dry = before;
  auto const planned = store_type{dry}.save(generation(phase + 1));
  if (!planned) return false;
  std::array<std::byte, record_bytes> record{};
  auto _ = encode_record(record,
                         generation(phase + 1),
                         Placement.magic,
                         planned->seq);

  // A save that starts a block erases it first, and with no power at all
  // that erase is what fails.
  auto const erases =
      ((phase % Placement.slot_count) % Placement.slots_per_block == 0);

  for (auto budget = 0uz; budget <= record_bytes; ++budget) {
    auto memory = before;
    memory.set_power_budget(budget);
    auto const cut = store_type{memory}.save(generation(phase + 1));
    memory.set_power_budget(Memory::unlimited);

    if (cut.has_value() != (budget == record_bytes)) return false;
    if (cut && ((cut->slot != planned->slot) || (cut->seq != planned->seq))) {
      return false;
    }
    if (!cut) {
      auto stage = save_stage::commit;
      if (budget < body_bytes) stage = save_stage::body;
      if ((budget == 0) && erases) stage = save_stage::erase;
      if (cut.error().stage != stage) return false;
      if (cut.error().cause != storage_fault::power_loss) return false;
    }

    if (newest) {
      auto const at = Placement.base + (newest->slot * Placement.slot_capacity);
      if (!std::ranges::equal(
              memory.bytes().subspan(at, Placement.slot_capacity),
              before.bytes().subspan(at, Placement.slot_capacity))) {
        return false;
      }
    }

    // Holds what no record on the medium holds, so whatever the load leaves
    // in it came from the load.
    auto restored = generation(phase + 2);
    store_type restarted{memory};
    auto const loaded = restarted.load(restored);
    if (loaded.read_failed) return false;

    if (nvm::is_erased<Memory>(std::span{record}.subspan(budget))) {
      if ((loaded.slot != planned->slot)
          || (loaded.record.seq != planned->seq)) {
        return false;
      }
      if (restored.get<"generation">() != phase + 1) return false;
    }
    else if (newest) {
      if ((loaded.slot != newest->slot)
          || (loaded.record.seq != newest->seq)) {
        return false;
      }
      if (restored.get<"generation">() != phase) return false;
    }
    else {
      if (loaded.record.valid || loaded.slot) return false;
      if (restored.get<"generation">() != 0) return false;
    }

    auto const clean = restarted.save(generation(phase + 2));
    if (!clean) return false;

    image<schema> again;
    auto const reloaded = store_type{memory}.load(again);
    if ((reloaded.slot != clean->slot) || (reloaded.record.seq != clean->seq)) {
      return false;
    }
    if (again.get<"generation">() != phase + 2) return false;
  }
  return true;
}

// The first save, onto a blank medium: a restart after a cut that left no
// whole record finds none and loads the defaults.
consteval bool test_power_lost_during_the_first_save()
{
  return power_lost_at_every_byte<fram, fram_placement>(0)
      && power_lost_at_every_byte<flash, flash_placement>(0);
}

static_assert(test_power_lost_during_the_first_save());

// The second: into the other slot of FRAM, and into the flash block the first
// one erased, which it does not erase again.
consteval bool test_power_lost_during_the_second_save()
{
  return power_lost_at_every_byte<fram, fram_placement>(1)
      && power_lost_at_every_byte<flash, flash_placement>(1);
}

static_assert(test_power_lost_during_the_second_save());

// The third: over the first record on FRAM, and into the next block on flash.
consteval bool test_power_lost_during_the_third_save()
{
  return power_lost_at_every_byte<fram, fram_placement>(2)
      && power_lost_at_every_byte<flash, flash_placement>(2);
}

static_assert(test_power_lost_during_the_third_save());

// The fourth: into the last slot.
consteval bool test_power_lost_during_the_fourth_save()
{
  return power_lost_at_every_byte<fram, fram_placement>(3)
      && power_lost_at_every_byte<flash, flash_placement>(3);
}

static_assert(test_power_lost_during_the_fourth_save());

// The fifth wraps around: on flash it erases the block of the first two
// records.
consteval bool test_power_lost_during_the_fifth_save()
{
  return power_lost_at_every_byte<fram, fram_placement>(4)
      && power_lost_at_every_byte<flash, flash_placement>(4);
}

static_assert(test_power_lost_during_the_fifth_save());

// The sixth goes on in the block the fifth erased.
consteval bool test_power_lost_during_the_sixth_save()
{
  return power_lost_at_every_byte<fram, fram_placement>(5)
      && power_lost_at_every_byte<flash, flash_placement>(5);
}

static_assert(test_power_lost_during_the_sixth_save());

// The ninth wraps around flash a second time.
consteval bool test_power_lost_during_the_ninth_save()
{
  return power_lost_at_every_byte<fram, fram_placement>(8)
      && power_lost_at_every_byte<flash, flash_placement>(8);
}

static_assert(test_power_lost_during_the_ninth_save());

} // namespace
