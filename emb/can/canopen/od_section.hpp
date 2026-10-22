#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <type_traits>

#include "od.hpp"
#include "od_handlers.hpp"

#include <emb/settings/image.hpp>
#include <emb/settings/record.hpp>
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
//
// What only a record has, a reader returns as `std::optional`, and an empty
// one answers `no_data_available`, 0800 0024h: the number of the record the
// section last restored or wrote, and of the report of the last load the
// slot, the number, whether the schema matched and the counts of values,
// which a load that restored no record leaves at their defaults. Zero, false
// and FFFFFFFFh were data a host could take for a record's. Whether the load
// restored a record (`valid`), whether the medium refused a read
// (`read_failed`), the flags and where the records are kept stay values:
// they are answers whatever the load found.
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

  // Returns `field` of the report of the record that the last load restored,
  // as a `T`, or nothing if the load restored none.
  template<typename T, typename Field>
  static constexpr std::optional<T>
  of_record(context_type& ctx, Field settings::load_report::* field)
  {
    auto const& record = SectionOf(ctx).last_load().record;
    if (!record.valid) return std::nullopt;
    return record.*field;
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

  // Returns the number of the record that the section last restored or
  // wrote (`settings::section::sequence`), or nothing while it has none.
  static constexpr std::optional<std::uint32_t> sequence(context_type& ctx)
  {
    return SectionOf(ctx).sequence();
  }

  // Returns the number of blocks that the saves since the section was last
  // empty have filled, i.e. `sequence() / slots_per_block()`, or nothing
  // with `sequence()`; zero if the medium needs no erasing, which no save
  // wears.
  static constexpr std::optional<std::uint32_t> erase_cycles(context_type& ctx)
  {
    if constexpr (section_type::storage_type::needs_erase) {
      return SectionOf(ctx).sequence().transform(
          [](std::uint32_t seq) { return seq / slots_per_block(); });
    }
    else {
      return std::uint32_t{0};
    }
  }

  // Returns the slot of the record the last load restored, or nothing if it
  // restored none.
  static constexpr std::optional<std::uint32_t> loaded_slot(context_type& ctx)
  {
    auto const& loaded = SectionOf(ctx).last_load();
    if (!loaded.slot) return std::nullopt;
    return as_u32(*loaded.slot);
  }

  static constexpr std::optional<std::uint32_t>
  loaded_sequence(context_type& ctx)
  {
    return of_record<std::uint32_t>(ctx, &settings::load_report::seq);
  }

  static constexpr bool valid(context_type& ctx)
  {
    return SectionOf(ctx).last_load().record.valid;
  }

  static constexpr std::optional<bool> schema_matched(context_type& ctx)
  {
    return of_record<bool>(ctx, &settings::load_report::schema_matched);
  }

  static constexpr bool read_failed(context_type& ctx)
  {
    return SectionOf(ctx).last_load().read_failed;
  }

  static constexpr std::optional<std::uint32_t> stored(context_type& ctx)
  {
    return of_record<std::uint32_t>(ctx, &settings::load_report::stored);
  }

  static constexpr std::optional<std::uint32_t> loaded(context_type& ctx)
  {
    return of_record<std::uint32_t>(ctx, &settings::load_report::loaded);
  }

  static constexpr std::optional<std::uint32_t> unknown(context_type& ctx)
  {
    return of_record<std::uint32_t>(ctx, &settings::load_report::unknown);
  }

  static constexpr std::optional<std::uint32_t> rejected(context_type& ctx)
  {
    return of_record<std::uint32_t>(ctx, &settings::load_report::rejected);
  }

  // Returns the number of parameters that the record restored carries no
  // value of, or nothing if the load restored none: then no parameter has a
  // value stored, and zero would say they all have.
  static constexpr std::optional<std::uint32_t> missing(context_type& ctx)
  {
    return of_record<std::uint32_t>(ctx, &settings::load_report::missing);
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
