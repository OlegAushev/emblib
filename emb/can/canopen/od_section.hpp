#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <type_traits>

#include "od_settings.hpp"

#include <emb/settings/image.hpp>
#include <emb/settings/section.hpp>
#include <emb/settings/value.hpp>

namespace emb {
namespace can {
namespace canopen {

namespace detail {

template<auto& Section>
using section_t = std::remove_cvref_t<decltype(Section)>;

template<auto& Section>
std::expected<settings::value, settings::error>
section_get_at(std::size_t index)
{
  return Section.get_at(index);
}

template<auto& Section>
std::expected<void, settings::error>
section_set_at(std::size_t index, settings::value const& value)
{
  return Section.set_at(index, value);
}

template<auto& Section>
std::expected<void, settings::error>
section_restore_default_at(std::size_t index)
{
  return Section.restore_default_at(index);
}

} // namespace detail

// The alias template `od_settings_for` is the `od_settings` bridge to the
// parameters of `Section`, a `settings::section`, through the by-index
// accessors of `Section`, which mark what a write changes.
template<auto& Section>
using od_settings_for =
    od_settings<detail::section_t<Section>::schema,
                &detail::section_get_at<Section>,
                &detail::section_set_at<Section>,
                &detail::section_restore_default_at<Section>>;

// The class template `od_section_status` provides readers, for `od_ro`, of
// the state of `Section`, a `settings::section`: where its records are kept,
// how far its saves have advanced around the slots, what the last load found
// and what changes are waiting to be applied.
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
    return Section.pending().restart_required();
  }

  static bool changes_pending()
  {
    return Section.pending().any();
  }
};

} // namespace canopen
} // namespace can
} // namespace emb
