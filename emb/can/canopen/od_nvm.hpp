#pragma once

#include "od.hpp"
#include "od_value_cast.hpp"

#include <emb/nvm.hpp>

#include <expected>
#include <type_traits>

namespace emb {
namespace can {
namespace canopen {

// Bridges an nvm::registry into OD entry handlers: read<Name>/write<Name>
// match the od_entry function pointer signatures and expose one registry
// parameter per name. RegistryFn is a callable NTTP — typically the
// application's accessor function — returning a reference to the registry;
// a function keeps the closure type out of every bridge symbol's mangling.
//
// Error mapping: corrupted or never-written storage (hash/CRC mismatch)
// reports no_data_available, everything else hardware_error.
template<auto RegistryFn>
  requires std::is_lvalue_reference_v<decltype(RegistryFn())>
struct od_nvm {
  using registry_type = std::remove_reference_t<decltype(RegistryFn())>;

  // Shared per-value-type bodies; per-name read/write below collapse to
  // building a parameter_ref and a tail call. Same deal as in emb::nvm:
  // noinline keeps the bodies from being inlined back into every per-name
  // instantiation.
  template<typename T>
  [[gnu::noinline]] static auto
  read_impl(typename registry_type::template parameter_ref<T> p)
      -> od_read_result {
    auto r = RegistryFn().get(p);
    if (!r) {
      using enum nvm::error;
      auto code = (r.error() == hash_mismatch || r.error() == crc_mismatch)
                    ? sdo_abort_code::no_data_available
                    : sdo_abort_code::hardware_error;
      return std::unexpected(code);
    }
    return to_od_value(*r);
  }

  template<typename T>
  [[gnu::noinline]] static auto
  write_impl(typename registry_type::template parameter_ref<T> p, od_value val)
      -> od_write_result {
    auto v = from_od_value<T>(val);
    if (!v) {
      return std::unexpected(sdo_abort_code::data_type_mismatch);
    }

    if (!RegistryFn().set(p, *v)) {
      return std::unexpected(sdo_abort_code::hardware_error);
    }
    return {};
  }

  template<nvm::parameter_name Name>
  static auto read() -> od_read_result {
    return read_impl(registry_type::template ref<Name>());
  }

  template<nvm::parameter_name Name>
  static auto write(od_value val) -> od_write_result {
    return write_impl(registry_type::template ref<Name>(), val);
  }
};

} // namespace canopen
} // namespace can
} // namespace emb
