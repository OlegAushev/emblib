#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <type_traits>

#include "od.hpp"
#include "od_handlers.hpp"

#include <emb/settings/image.hpp>
#include <emb/settings/section.hpp>
#include <emb/settings/value.hpp>

namespace emb {
namespace can {
namespace canopen {

// The class template `od_section_status` provides readers, for `od_ro`, of
// the state of the `settings::section` that `SectionOf` returns from a
// reference to the context of the dictionary, as `od_settings` reaches it:
// where its records are kept, how far its saves have advanced around the
// slots, what the last load found, and whether values wait to be applied, to
// be saved or for a restart. The readers of where the records are kept take
// no context, since the type of the section tells.
template<auto SectionOf>
  requires detail::od_context_function<SectionOf>
        && std::is_lvalue_reference_v<detail::od_result_t<SectionOf>>
class od_section_status {
  using context_type = detail::od_context_of<SectionOf>;
  using section_type = std::remove_cvref_t<detail::od_result_t<SectionOf>>;

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

  static constexpr std::uint32_t sequence(context_type& ctx)
  {
    return SectionOf(ctx).sequence();
  }

  // Returns the number of blocks that the saves since the section was last
  // empty have filled, i.e. `sequence() / slots_per_block()`, or zero if the
  // medium needs no erasing.
  static constexpr std::uint32_t erase_cycles(context_type& ctx)
  {
    if constexpr (section_type::storage_type::needs_erase) {
      return SectionOf(ctx).sequence() / slots_per_block();
    }
    else {
      return 0;
    }
  }

  // Returns the slot of the record the last load restored, or `0xFFFFFFFF`
  // if it restored none.
  static constexpr std::uint32_t loaded_slot(context_type& ctx)
  {
    auto const& loaded = SectionOf(ctx).last_load();
    return loaded.slot ? as_u32(*loaded.slot) : ~std::uint32_t{0};
  }

  static constexpr std::uint32_t loaded_sequence(context_type& ctx)
  {
    return SectionOf(ctx).last_load().record.seq;
  }

  static constexpr bool valid(context_type& ctx)
  {
    return SectionOf(ctx).last_load().record.valid;
  }

  static constexpr bool schema_matched(context_type& ctx)
  {
    return SectionOf(ctx).last_load().record.schema_matched;
  }

  static constexpr bool read_failed(context_type& ctx)
  {
    return SectionOf(ctx).last_load().read_failed;
  }

  static constexpr std::uint32_t stored(context_type& ctx)
  {
    return SectionOf(ctx).last_load().record.stored;
  }

  static constexpr std::uint32_t loaded(context_type& ctx)
  {
    return SectionOf(ctx).last_load().record.loaded;
  }

  static constexpr std::uint32_t unknown(context_type& ctx)
  {
    return SectionOf(ctx).last_load().record.unknown;
  }

  static constexpr std::uint32_t rejected(context_type& ctx)
  {
    return SectionOf(ctx).last_load().record.rejected;
  }

  static constexpr std::uint32_t missing(context_type& ctx)
  {
    return SectionOf(ctx).last_load().record.missing;
  }

  static constexpr bool restart_required(context_type& ctx)
  {
    return SectionOf(ctx).restart_required();
  }

  static constexpr bool unapplied(context_type& ctx)
  {
    return SectionOf(ctx).unapplied();
  }

  static constexpr bool unsaved(context_type& ctx)
  {
    return SectionOf(ctx).unsaved();
  }
};

} // namespace canopen
} // namespace can
} // namespace emb
