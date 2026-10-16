#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <type_traits>

#include "od.hpp"

#include <emb/settings/image.hpp>
#include <emb/settings/section.hpp>
#include <emb/settings/value.hpp>

namespace emb {
namespace can {
namespace canopen {

namespace detail {

template<auto& Section>
using section_t = std::remove_cvref_t<decltype(Section)>;

} // namespace detail

// The class template `od_section_status` provides readers, for `od_ro`, of
// the state of `Section`, a `settings::section`: where its records are kept,
// how far its saves have advanced around the slots, what the last load found,
// and whether values wait to be applied, to be saved or for a restart.
template<auto& Section>
class od_section_status {
  using section_type = detail::section_t<Section>;

  static constexpr auto const& placement = section_type::placement;

  static constexpr std::uint32_t as_u32(std::size_t v)
  {
    return static_cast<std::uint32_t>(v);
  }

public:
  static constexpr std::uint32_t magic()
  {
    return placement.magic;
  }

  static constexpr std::uint32_t slot_count()
  {
    return as_u32(placement.slot_count);
  }

  static constexpr std::uint32_t slot_capacity()
  {
    return as_u32(placement.slot_capacity);
  }

  static constexpr std::uint32_t slots_per_block()
  {
    return as_u32(placement.slots_per_block);
  }

  static std::uint32_t sequence()
  {
    return Section.sequence();
  }

  // Returns the number of blocks that the saves since the section was last
  // empty have filled, i.e. `sequence() / slots_per_block()`, or zero if the
  // medium needs no erasing.
  static std::uint32_t erase_cycles()
  {
    if constexpr (section_type::storage_type::needs_erase) {
      return Section.sequence() / slots_per_block();
    }
    else {
      return 0;
    }
  }

  // Returns the slot of the record the last load restored, or `0xFFFFFFFF`
  // if it restored none.
  static std::uint32_t loaded_slot()
  {
    auto const& loaded = Section.last_load();
    return loaded.slot ? as_u32(*loaded.slot) : ~std::uint32_t{0};
  }

  static std::uint32_t loaded_sequence()
  {
    return Section.last_load().record.seq;
  }

  static bool valid()
  {
    return Section.last_load().record.valid;
  }

  static bool schema_matched()
  {
    return Section.last_load().record.schema_matched;
  }

  static bool read_failed()
  {
    return Section.last_load().read_failed;
  }

  static std::uint32_t stored()
  {
    return Section.last_load().record.stored;
  }

  static std::uint32_t loaded()
  {
    return Section.last_load().record.loaded;
  }

  static std::uint32_t unknown()
  {
    return Section.last_load().record.unknown;
  }

  static std::uint32_t rejected()
  {
    return Section.last_load().record.rejected;
  }

  static std::uint32_t missing()
  {
    return Section.last_load().record.missing;
  }

  static bool restart_required()
  {
    return Section.restart_required();
  }

  static bool unapplied()
  {
    return Section.unapplied();
  }

  static bool unsaved()
  {
    return Section.unsaved();
  }
};

} // namespace canopen
} // namespace can
} // namespace emb
