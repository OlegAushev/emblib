#pragma once

#include <emb/meta/fixed_string.hpp>
#include <emb/settings/schema.hpp>

#include <array>
#include <expected>

#include <cstddef>
#include <cstdint>

namespace emb {
namespace settings {

// Why a value could not be read or written through the erased interface.
// Storage failures are not here: they belong to the store, which is the
// only layer that talks to a medium.
enum class error : std::uint8_t {
  unknown_parameter,
  read_only,
  type_mismatch,
  out_of_range,
};

// The working copy of every parameter: one four-byte cell per declaration,
// in declaration order, so an index addresses the same value here, in the
// schema and in a stored record.
//
// Plain data on purpose. It is not a synchronization primitive and holds no
// atomics — that is what keeps it usable in constant expressions, and the
// contexts that share one are the application's business, not the library's.
template<auto& Schema>
class image {
  using schema_type = schema_t<Schema>;

  std::array<raw_value, schema_type::count> cells_{};

public:
  static constexpr std::size_t count = schema_type::count;

  constexpr image()
  {
    restore_defaults();
  }

  // -- Access by name --
  //
  // The application's own path: the static type is preserved, and
  // `writable` is not consulted. A parameter closed to a protocol may still
  // be written by the code that owns it — a calibration result, a value
  // recorded in production.

  template<fixed_string Name>
  constexpr auto get() const -> typename parameter<Schema, Name>::type
  {
    using param_type = parameter<Schema, Name>;
    return from_raw<typename param_type::type>(cells_[param_type::index]);
  }

  template<fixed_string Name>
  constexpr auto set(typename parameter<Schema, Name>::type const& v)
      -> std::expected<change, error>
  {
    using param_type = parameter<Schema, Name>;
    return write(param_type::index, to_raw(v));
  }

  template<fixed_string Name>
  constexpr auto restore_default() -> std::expected<change, error>
  {
    using param_type = parameter<Schema, Name>;
    return write(param_type::index, param_type::desc.def);
  }

  // -- Access by index --
  //
  // The path a transport takes: nothing static is known, so every rule the
  // schema states is enforced — including `writable`, which the by-name
  // path above deliberately ignores. Spelled apart from get/set for that
  // reason: same operation, different promise.

  constexpr auto get_at(std::size_t index) const -> std::expected<value, error>
  {
    if (index >= count) {
      return std::unexpected(error::unknown_parameter);
    }
    return to_value(Schema.parameters[index].type, cells_[index]);
  }

  constexpr auto set_at(std::size_t index, value const& v)
      -> std::expected<change, error>
  {
    if (index >= count) {
      return std::unexpected(error::unknown_parameter);
    }

    auto const& desc = Schema.parameters[index];
    if (!desc.writable) {
      return std::unexpected(error::read_only);
    }
    if (held_type(v) != desc.type) {
      return std::unexpected(error::type_mismatch);
    }

    return write(index, to_raw(v));
  }

  constexpr auto restore_default_at(std::size_t index)
      -> std::expected<change, error>
  {
    if (index >= count) {
      return std::unexpected(error::unknown_parameter);
    }
    return write(index, Schema.parameters[index].def);
  }

  constexpr void restore_defaults()
  {
    for (auto i = 0uz; i < count; ++i)
      cells_[i] = Schema.parameters[i].def;
  }

  // -- Cells --
  //
  // What the store reads and fills. Both take an index the caller has
  // already checked; a record loader looks it up in the schema, and a
  // record writer walks the whole image.

  constexpr auto cell(std::size_t index) const -> raw_value
  {
    return cells_[index];
  }

  // Named apart from set_at, and with a verb of its own, because it
  // promises less: no validation and no change reported. A loader checks a
  // cell against its descriptor before accepting it, and a load is not a
  // change to apply — it is where the values came from.
  constexpr void assign_cell(std::size_t index, raw_value cell)
  {
    cells_[index] = cell;
  }

private:
  constexpr auto write(std::size_t index, raw_value cell)
      -> std::expected<change, error>
  {
    auto const& desc = Schema.parameters[index];
    if (!in_range(desc.type, cell, desc.min, desc.max)) {
      return std::unexpected(error::out_of_range);
    }

    cells_[index] = cell;
    return change{desc.group, desc.apply};
  }
};

} // namespace settings
} // namespace emb
