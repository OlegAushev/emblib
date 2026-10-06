#pragma once

#include <emb/meta/fixed_string.hpp>
#include <emb/settings/schema.hpp>

#include <array>
#include <expected>

#include <cstddef>
#include <cstdint>

namespace emb {
namespace settings {

// The scoped enumeration `error` defines the reasons for which the members of
// `image` refuse to read or write a parameter.
enum class error : std::uint8_t {
  // Index that names no parameter, returned by `image::get_at`,
  // `image::set_at` and `image::restore_default_at`.
  unknown_parameter,
  // Write by index to a parameter that is not `writable`, returned by
  // `image::set_at` and `image::restore_default_at`.
  read_only,
  // `value` holding another alternative than the scalar type of the
  // parameter, returned by `image::set_at`.
  type_mismatch,
  // Value outside the bounds of the parameter, or a NaN, returned by
  // `image::set` and `image::set_at`.
  out_of_range,
};

// The class template `image` holds the working copy of the values of the
// parameters `Schema` declares: one cell per parameter, in declaration order,
// so that the index of a parameter in `Schema.parameters` addresses its cell.
// On construction, every cell holds the default of its parameter.
//
// A write through `set`, `restore_default`, `set_at` or `restore_default_at`
// that is refused returns an `error` and leaves the cell as it was.
//
// An `image` holds no atomics and takes no locks: contexts that share one,
// e.g. a task and an interrupt handler, must keep a write from overlapping
// any other access to it themselves. An `image` is usable in constant
// expressions.
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

  template<fixed_string Name>
  constexpr typename parameter<Schema, Name>::type get() const
  {
    using param_type = parameter<Schema, Name>;
    return from_raw<typename param_type::type>(cells_[param_type::index]);
  }

  // Writes `v` to the parameter `Name` whether or not it is `writable`: the
  // code that owns a parameter closed to protocols, e.g. a calibration result
  // or a value recorded in production, writes it here. Returns
  // `error::out_of_range` if `v` is outside the bounds of the parameter.
  template<fixed_string Name>
  constexpr std::expected<void, error>
  set(typename parameter<Schema, Name>::type const& v)
  {
    using param_type = parameter<Schema, Name>;
    return write(param_type::index, to_raw(v));
  }

  // Writes the default of the parameter `Name`, as `set` does.
  template<fixed_string Name>
  constexpr std::expected<void, error> restore_default()
  {
    using param_type = parameter<Schema, Name>;
    return write(param_type::index, param_type::desc.def);
  }

  // -- Access by index --

  // Returns the value of the parameter at `index`, or
  // `error::unknown_parameter` if `index >= count`.
  constexpr std::expected<value, error> get_at(std::size_t index) const
  {
    if (index >= count) {
      return std::unexpected(error::unknown_parameter);
    }
    return to_value(Schema.parameters[index].type, cells_[index]);
  }

  // Writes `v` to the parameter at `index`. Returns
  // `error::unknown_parameter` if `index >= count`, otherwise
  // `error::read_only` if the parameter is not `writable`, otherwise
  // `error::type_mismatch` if `v` holds another alternative than the scalar
  // type of the parameter, otherwise `error::out_of_range` if `v` is outside
  // the bounds of the parameter.
  constexpr std::expected<void, error> set_at(std::size_t index, value const& v)
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

  // Writes the default of the parameter at `index`. Returns
  // `error::unknown_parameter` if `index >= count`, otherwise
  // `error::read_only` if the parameter is not `writable`: what a protocol may
  // not write, it may not reset either.
  constexpr std::expected<void, error> restore_default_at(std::size_t index)
  {
    if (index >= count) {
      return std::unexpected(error::unknown_parameter);
    }

    auto const& desc = Schema.parameters[index];
    if (!desc.writable) {
      return std::unexpected(error::read_only);
    }

    return write(index, desc.def);
  }

  // Assigns every parameter its default, whether or not it is `writable`.
  constexpr void restore_defaults()
  {
    for (auto i = 0uz; i < count; ++i)
      cells_[i] = Schema.parameters[i].def;
  }

  // -- Cells --

  // Returns the cell of the parameter at `index`. The behavior is undefined
  // if `index >= count`.
  constexpr raw_value cell(std::size_t index) const
  {
    return cells_[index];
  }

  // Assigns `cell` to the parameter at `index` as it is, i.e. without
  // checking it against the bounds of the parameter. The behavior is
  // undefined if `index >= count`.
  constexpr void assign_cell(std::size_t index, raw_value cell)
  {
    cells_[index] = cell;
  }

private:
  // Returns `error::out_of_range` if `cell` is outside the bounds of the
  // parameter at `index`; otherwise writes it there. The behavior is
  // undefined if `index >= count`.
  constexpr std::expected<void, error> write(std::size_t index, raw_value cell)
  {
    auto const& desc = Schema.parameters[index];
    if (!in_range(desc.type, cell, desc.min, desc.max)) {
      return std::unexpected(error::out_of_range);
    }
    cells_[index] = cell;
    return {};
  }
};

} // namespace settings
} // namespace emb
