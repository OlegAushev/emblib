#include <array>
#include <cstddef>
#include <cstdint>

#include <emb/settings/store.hpp>

namespace {

using namespace emb::settings;

// Numbers of the newest record, taken in turn from one view to the next:
// zero, below which the older records wrap; the middle and the top of the
// counter; and its last value, past which the next record wraps through zero.
inline constexpr std::array<std::uint32_t, 4> newest_seqs{0u,
                                                          0x7FFFFFF0u,
                                                          0xFFFFFFF0u,
                                                          0xFFFFFFFFu};

// Slots of a view that take every class: all of them, or only those after
// the newest record in its block, the only ones the plan reads.
enum class varied : std::uint8_t {
  all,
  window,
};

// Checks that `v` has its newest record in `newest` and that the save planned
// on top of it spares that record: it takes the first erased slot after the
// record in its block, or else erases the next block and takes its first
// slot, and numbers the record by its distance from the newest on the ring.
// Once written, the record is the newest.
template<std::size_t SlotCount, std::size_t SlotsPerBlock>
consteval bool
plans_past(typename detail::slot_ring<SlotCount, SlotsPerBlock>::view const& v,
           std::size_t newest)
{
  using slot_ring = detail::slot_ring<SlotCount, SlotsPerBlock>;

  if (slot_ring::newest(v) != newest) return false;

  auto const p = slot_ring::plan(v);
  auto const block = newest / SlotsPerBlock;
  if (p.slot == newest) return false;
  if (p.erase_block && (p.slot / SlotsPerBlock == block)) return false;

  if (p.erase_block) {
    auto const next = (block + 1) % slot_ring::block_count;
    if (p.slot != next * SlotsPerBlock) return false;
  }
  else {
    if ((p.slot / SlotsPerBlock != block) || (p.slot < newest)) return false;
    if (!v.erased[p.slot]) return false;
  }

  auto const passed = p.erase_block ? (block + 1) * SlotsPerBlock : p.slot;
  for (auto slot = newest + 1; slot < passed; ++slot)
    if (v.erased[slot]) return false;

  auto const distance = (p.slot + SlotCount - newest) % SlotCount;
  if (p.seq != v.seq[newest] + static_cast<std::uint32_t>(distance)) {
    return false;
  }

  auto after = v;
  slot_ring::apply(after, p);
  if (slot_ring::newest(after) != p.slot) return false;
  if (!after.whole[newest]) return false;

  return true;
}

// Checks every view whose newest record lies in a slot of [`first`, `last`):
// each other slot erased, holding debris or whole, as `kinds` allows. Where
// it allows only whole or not, a slot that is not whole is erased in one view
// and holds debris in the next. The whole slots are numbered as this store
// numbers its own records, one less for each slot they lie behind the newest
// on the ring. The others carry a number newer than any, which must count
// for nothing.
template<std::size_t SlotCount, std::size_t SlotsPerBlock>
consteval bool every_view(varied kinds,
                          std::size_t first = 0,
                          std::size_t last = SlotCount)
{
  auto turn = 0uz;
  for (auto newest = first; newest < last; ++newest) {
    auto const end = (newest / SlotsPerBlock + 1) * SlotsPerBlock;
    auto const classes = [&](std::size_t slot) {
      auto const in_window = (slot > newest) && (slot < end);
      return ((kinds == varied::all) || in_window) ? 3uz : 2uz;
    };

    auto views = 1uz;
    for (auto slot = 0uz; slot < SlotCount; ++slot)
      if (slot != newest) views *= classes(slot);

    for (auto code = 0uz; code < views; ++code, ++turn) {
      auto const seq = newest_seqs[turn % newest_seqs.size()];
      typename detail::slot_ring<SlotCount, SlotsPerBlock>::view v;

      for (auto slot = 0uz, rest = code; slot < SlotCount; ++slot) {
        auto whole = (slot == newest);
        if (!whole) {
          auto const digit = rest % classes(slot);
          rest /= classes(slot);
          whole = (digit == classes(slot) - 1);
          if ((digit == 0) && ((classes(slot) == 3) || (turn % 2 == 0))) {
            v.erased.set(slot);
          }
        }

        auto const behind = static_cast<std::uint32_t>(
            (newest + SlotCount - slot) % SlotCount);
        if (whole) v.whole.set(slot);
        v.seq[slot] = whole ? seq - behind : seq + 1u;
      }

      if (!plans_past<SlotCount, SlotsPerBlock>(v, newest)) return false;
    }
  }
  return true;
}

// Checks every view with no whole record, each slot erased or holding
// debris: the save goes to slot 0, erases its block, is numbered 1, and once
// written is the newest.
template<std::size_t SlotCount, std::size_t SlotsPerBlock>
consteval bool no_record()
{
  using slot_ring = detail::slot_ring<SlotCount, SlotsPerBlock>;

  for (auto code = 0uz; code < (1uz << SlotCount); ++code) {
    typename slot_ring::view v;
    for (auto slot = 0uz; slot < SlotCount; ++slot) {
      if (((code >> slot) & 1) != 0) v.erased.set(slot);
      v.seq[slot] = 9;
    }

    if (slot_ring::newest(v)) return false;
    auto const p = slot_ring::plan(v);
    if ((p.slot != 0) || !p.erase_block || (p.seq != 1)) return false;
    slot_ring::apply(v, p);
    if (slot_ring::newest(v) != 0) return false;
  }
  return true;
}

// -- Every view --

// Two slots of one each, the FRAM of the product: every save fills the slot
// it takes, the one that does not hold the newest record.
consteval bool test_two_slots_in_blocks_of_one()
{
  return every_view<2, 1>(varied::all) && no_record<2, 1>();
}

static_assert(test_two_slots_in_blocks_of_one());

consteval bool test_four_slots_in_blocks_of_one()
{
  return every_view<4, 1>(varied::all) && no_record<4, 1>();
}

static_assert(test_four_slots_in_blocks_of_one());

// The flash of the store tests: a save takes the second slot of a block
// without erasing it, if that slot is erased.
consteval bool test_four_slots_in_blocks_of_two()
{
  return every_view<4, 2>(varied::all) && no_record<4, 2>();
}

static_assert(test_four_slots_in_blocks_of_two());

// Three blocks: the block a save erases is the one after the newest record's,
// not merely another one.
consteval bool test_six_slots_in_blocks_of_two()
{
  return every_view<6, 2>(varied::all) && no_record<6, 2>();
}

static_assert(test_six_slots_in_blocks_of_two());

consteval bool test_six_slots_in_blocks_of_three()
{
  return every_view<6, 3>(varied::all) && no_record<6, 3>();
}

static_assert(test_six_slots_in_blocks_of_three());

// Two blocks of four, the flash of the product in small: up to three slots
// lie after the newest record in its block. Every combination of them is
// checked; of the other slots, only whether each is whole.
consteval bool test_eight_slots_with_the_newest_in_the_first_block()
{
  return every_view<8, 4>(varied::window, 0, 4) && no_record<8, 4>();
}

static_assert(test_eight_slots_with_the_newest_in_the_first_block());

consteval bool test_eight_slots_with_the_newest_in_the_second_block()
{
  return every_view<8, 4>(varied::window, 4, 8);
}

static_assert(test_eight_slots_with_the_newest_in_the_second_block());

// -- The newest record --

// Two whole records with one number, which an older store could leave: the
// one in the lower slot is the newest, as it was there.
consteval bool test_equal_numbers_take_the_lower_slot()
{
  using slot_ring = detail::slot_ring<4, 2>;
  slot_ring::view v;
  v.whole.set(1);
  v.whole.set(3);
  v.seq[1] = 7;
  v.seq[3] = 7;
  if (slot_ring::newest(v) != 1) return false;

  v.whole.set(0);
  v.whole.set(2);
  v.seq[0] = 6;
  v.seq[2] = 6;
  if (slot_ring::newest(v) != 1) return false;

  return true;
}

static_assert(test_equal_numbers_take_the_lower_slot());

// The counter wraps through zero: 0 and 1 follow 0xFFFFFFFF, wherever the
// ring holds them.
consteval bool test_a_counter_that_wrapped()
{
  using slot_ring = detail::slot_ring<4, 1>;
  slot_ring::view v;
  v.whole.set();

  v.seq = {0xFFFFFFFEu, 0xFFFFFFFFu, 0u, 1u};
  if (slot_ring::newest(v) != 3) return false;

  v.seq = {0u, 1u, 0xFFFFFFFEu, 0xFFFFFFFFu};
  if (slot_ring::newest(v) != 1) return false;

  return true;
}

static_assert(test_a_counter_that_wrapped());

// 0x80000005 is half the counter away from 5, so neither is newer than the
// other, and no record is newer than all the rest. Only an older store
// numbered records like that, past a header whose bit 31 had rotted; the
// highest number stands in for the newest.
consteval bool test_no_record_newer_than_all_the_others()
{
  using slot_ring = detail::slot_ring<4, 1>;
  slot_ring::view v;
  v.whole.set(0);
  v.whole.set(1);
  v.whole.set(2);
  v.seq[0] = 5;
  v.seq[1] = 0x80000005u;
  v.seq[2] = 6;

  if (slot_ring::newest(v) != 1) return false;

  return true;
}

static_assert(test_no_record_newer_than_all_the_others());

// A save that erases a block takes the records in it along. Where an older
// store numbered records past a rotted bit 31, they can decide which record
// is the newest: once 0x80000003 is gone with its block, the record planned
// after seq 2 is the newest; once 3 is gone with its block, the record
// numbered 1 is newer than the rest, the planned one included.
consteval bool test_apply_clears_the_block_it_erases()
{
  using slot_ring = detail::slot_ring<4, 2>;

  {
    slot_ring::view v;
    v.erased.set(0);
    v.erased.set(2);
    v.whole.set(1);
    v.whole.set(3);
    v.seq[1] = 2;
    v.seq[3] = 0x80000003u;

    auto const p = slot_ring::plan(v);
    if ((p.slot != 2) || !p.erase_block || (p.seq != 3)) return false;
    slot_ring::apply(v, p);
    if (v.whole[3] || !v.erased[3]) return false;
    if (slot_ring::newest(v) != 2) return false;
  }

  slot_ring::view v;
  v.erased.set(0);
  v.whole.set(1);
  v.whole.set(2);
  v.whole.set(3);
  v.seq[1] = 3;
  v.seq[2] = 1;
  v.seq[3] = 0x80000003u;

  auto const p = slot_ring::plan(v);
  if ((p.slot != 0) || !p.erase_block || (p.seq != 0x80000004u)) return false;
  slot_ring::apply(v, p);
  if (v.whole[1] || !v.erased[1]) return false;
  if (slot_ring::newest(v) != 2) return false;

  return true;
}

static_assert(test_apply_clears_the_block_it_erases());

// An empty section: no record, so the first save goes to slot 0, erasing its
// block, and is numbered 1.
consteval bool test_an_empty_section()
{
  using slot_ring = detail::slot_ring<4, 2>;
  slot_ring::view const v;

  if (slot_ring::newest(v)) return false;
  auto const p = slot_ring::plan(v);
  if ((p.slot != 0) || !p.erase_block || (p.seq != 1)) return false;

  return true;
}

static_assert(test_an_empty_section());

} // namespace
