#pragma once

// NVM parameter registry. A layout is a constexpr array of {name, default}
// entries over a closed typelist of value types; each entry owns one storage
// cell laid out as hash | value | crc. The registry reads and writes cells by
// name (or index) with hash and CRC checks, on any byte-addressed backend
// satisfying some_storage.

#include <emb/meta.hpp>

#include <algorithm>
#include <array>
#include <bit>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <string_view>
#include <type_traits>
#include <variant>

namespace emb {
namespace nvm {

enum class error {
  timeout,
  bus_error,
  invalid_argument,
  access_denied,
  hash_mismatch,
  crc_mismatch,
};

// Structural string for naming a parameter as a template argument:
// registry::get<"motor.p">().
template<std::size_t N>
struct parameter_name {
  char data[N]{};

  constexpr parameter_name(char const (&str)[N])
  {
    std::copy_n(str, N, data);
  }

  constexpr std::size_t size() const
  {
    return N - 1;
  }

  constexpr std::string_view view() const
  {
    return {data, N - 1};
  }
};

template<std::size_t N>
parameter_name(char const (&)[N]) -> parameter_name<N>;

// -- Hash function objects --

struct fnv1a_32 {
  using type = std::uint32_t;

  static constexpr type operator()(std::string_view s)
  {
    type h = 0x811C9DC5u;
    for (char c : s) {
      h ^= static_cast<std::uint8_t>(c);
      h *= 0x01000193u;
    }
    return h;
  }
};

// -- CRC function objects --

struct crc32 {
  using type = std::uint32_t;

  template<typename Hash, typename Value>
  static constexpr type operator()(Hash const& h, Value const& val)
  {
    auto hb = std::bit_cast<std::array<std::uint8_t, sizeof(Hash)>>(h);
    auto vb = std::bit_cast<std::array<std::uint8_t, sizeof(Value)>>(val);
    type crc = 0xFFFFFFFFu;
    for (auto b : hb)
      crc = update(crc, b);
    for (auto b : vb)
      crc = update(crc, b);
    return crc ^ 0xFFFFFFFFu;
  }

private:
  static constexpr type update(type crc, std::uint8_t b)
  {
    crc ^= b;
    for (int i = 0; i < 8; ++i)
      crc = (crc >> 1) ^ (0xEDB88320u & (-(crc & 1u)));
    return crc;
  }
};

// -- Concepts --

template<typename F>
concept some_hash_fn = requires { typename F::type; }
                    && requires(F f, std::string_view s) {
                         { f(s) } -> std::same_as<typename F::type>;
                       };

template<typename F>
concept some_crc_fn = requires { typename F::type; }
                   && requires(F f, std::uint32_t h, std::uint32_t v) {
                        { f(h, v) } -> std::same_as<typename F::type>;
                      };

// -- Value types --

namespace detail {

template<typename... Ts>
consteval bool storable(typelist<Ts...>)
{
  return sizeof...(Ts) > 0
      && (std::is_trivially_copyable_v<Ts> && ...)
      && typelist_unique<typelist<Ts...>>;
}

// Message for the unknown-name static_assert: names the offending parameter
// in the error line itself instead of leaving it buried in the instantiation
// chain.
template<parameter_name Name>
consteval auto unknown_parameter_message()
{
  constexpr std::string_view prefix = "Unknown parameter '";
  std::array<char, prefix.size() + Name.size() + 1> msg{};
  auto it = std::copy_n(prefix.data(), prefix.size(), msg.begin());
  it = std::copy_n(Name.data, Name.size(), it);
  *it = '\'';
  return msg;
}

} // namespace detail

// The closed set of value types a layout may hold: an emb::typelist of
// distinct trivially copyable types. Closed because a homogeneous array
// cannot carry heterogeneous defaults; each entry stores its default in a
// variant over the set, and the alternative index is what recovers the
// parameter's static type at compile time.
template<typename T>
concept some_value_types = some_typelist<T> && detail::storable(T{});

// -- Parameter --

// One layout entry.
template<some_value_types Types>
struct parameter;

template<typename... Ts>
struct parameter<typelist<Ts...>> {
  // The default is a member with a converting constructor rather than a
  // constructor of parameter itself, and parameter stays an aggregate: an
  // aggregate member's initializer is an expression with a source location,
  // so a wrong default type is reported at the offending entry, not at the
  // whole list. Exactly the listed types — no promotions, so `0.05` where
  // `0.05f` was meant is an error, not a silently double-typed parameter.
  struct default_type {
    std::variant<Ts...> value;

    template<same_as_any<Ts...> T>
    constexpr default_type(T v) : value(std::in_place_type<T>, v)
    {
    }

    template<typename T>
    constexpr default_type(T) = delete (
        "NVM parameter default must have exactly one of the layout's value "
        "types: a literal needs its suffix (0.05f, not 0.05) and an integer "
        "its type (std::int32_t{1}, not 1)");

    constexpr std::size_t index() const
    {
      return value.index();
    }
  };

  std::string_view name;
  default_type default_value;

  constexpr std::size_t type_index() const
  {
    return default_value.index();
  }

  constexpr std::size_t size() const
  {
    static constexpr std::array<std::size_t, sizeof...(Ts)> sizes{
        sizeof(Ts)...};
    return sizes[default_value.index()];
  }
};

// -- Layout --

// Never defined: calling it from a consteval context fails constant
// evaluation with this name in the diagnostic.
void parameter_index_out_of_range();

template<some_hash_fn HashFn,
         some_crc_fn CrcFn,
         some_value_types Types,
         std::size_t N>
struct basic_layout {
  using hash_fn = HashFn;
  using crc_fn = CrcFn;
  using hash_type = typename HashFn::type;
  using crc_type = typename CrcFn::type;
  using types = Types;
  using parameter_type = parameter<Types>;

  static constexpr std::size_t count = N;
  static constexpr std::size_t npos = SIZE_MAX;
  static constexpr std::size_t overhead = sizeof(hash_type) + sizeof(crc_type);

  std::array<parameter_type, N> parameters;

  // Lookups are consteval members over the object's own data; the typed
  // accessors (value_type_at, default_at) live outside the class because a
  // member function cannot read its object in a constant expression (`this`
  // is not usable there), yet the alternative index must be a constant to
  // name the type.
  consteval std::size_t index_of(std::string_view name) const
  {
    for (auto i = 0uz; i < count; ++i)
      if (parameters[i].name == name) return i;
    return npos;
  }

  consteval hash_type hash_at(std::size_t index) const
  {
    if (index >= count) parameter_index_out_of_range();
    return HashFn{}(parameters[index].name);
  }

  // Offset of the entry's cell from the start of the layout; where the layout
  // sits in storage is the registry's business (its Base parameter).
  consteval std::size_t offset_of(std::size_t index) const
  {
    if (index >= count) parameter_index_out_of_range();
    std::size_t off = 0;
    for (auto i = 0uz; i < index; ++i)
      off += parameters[i].size() + overhead;
    return off;
  }

  consteval std::size_t size() const
  {
    std::size_t s = 0;
    for (auto i = 0uz; i < count; ++i)
      s += parameters[i].size() + overhead;
    return s;
  }

  // Stored hashes identify cells at runtime, so they — not just the names —
  // must be unique.
  consteval bool names_unique() const
  {
    std::array<hash_type, N> hashes{};
    for (auto i = 0uz; i < count; ++i)
      hashes[i] = hash_at(i);
    for (auto i = 0uz; i < count; ++i)
      for (auto j = i + 1; j < count; ++j)
        if (hashes[i] == hashes[j]) return false;
    return true;
  }
};

template<some_value_types Types, std::size_t N>
using layout = basic_layout<fnv1a_32, crc32, Types, N>;

// make_layout<types>({{"a", 1.0f}, {"b", true}, ...}): N is deduced from the
// braced list, each element converts through parameter's constructor.
template<some_value_types Types,
         some_hash_fn HashFn = fnv1a_32,
         some_crc_fn CrcFn = crc32,
         std::size_t N>
consteval auto make_layout(parameter<Types> const (&parameters)[N])
    -> basic_layout<HashFn, CrcFn, Types, N>
{
  return {std::to_array(parameters)};
}

// Static type and default of the I-th entry of a layout object.
template<auto& Layout, std::size_t I>
using value_type_at =
    typelist_at_t<typename std::remove_cvref_t<decltype(Layout)>::types,
                  Layout.parameters[I].type_index()>;

template<auto& Layout, std::size_t I>
consteval auto default_at() -> value_type_at<Layout, I>
{
  return std::get<value_type_at<Layout, I>>(
      Layout.parameters[I].default_value.value);
}

// -- Storage backend concept --

template<typename T>
concept some_storage = requires {
  typename T::addr_type;
  { T::capacity } -> std::convertible_to<std::size_t>;
} && requires(T& s, typename T::addr_type addr) {
  {
    s.template read<std::uint32_t>(addr)
  } -> std::same_as<std::expected<std::uint32_t, nvm::error>>;
  {
    s.template write<std::uint32_t>(addr, std::uint32_t{})
  } -> std::same_as<std::expected<void, nvm::error>>;
};

// -- Registry --

// Binds a layout to a storage backend at byte offset Base.
template<some_storage Storage, auto& Layout, std::size_t Base = 0>
class registry {
  Storage& storage_;

  using layout_type = std::remove_cvref_t<decltype(Layout)>;
  using hash_type = typename layout_type::hash_type;
  using crc_type = typename layout_type::crc_type;
  using crc_fn = typename layout_type::crc_fn;
  using addr_type = typename Storage::addr_type;

public:
  // Per-name facts of one layout entry. A nested class rather than alias
  // templates: aliases are transparent to name mangling, so spelling the
  // value type through value_type_at<> in get/set signatures would embed
  // the lookup expression — the layout object, its array type, the whole
  // typelist — into every mangled symbol, and binutils cannot even
  // demangle those. A nested class mangles by name, keeping symbols O(name).
  template<parameter_name Name>
  struct parameter {
    static constexpr std::size_t lookup = Layout.index_of(Name.view());
    static_assert(lookup != layout_type::npos,
                  detail::unknown_parameter_message<Name>());
    // Clamped so a wrong name produces the static_assert alone: with npos
    // fed into the accessors below, GCC adds an out-of-bounds std::array
    // error and a "no matching function for get" on top of it.
    static constexpr std::size_t index = lookup == layout_type::npos ? 0
                                                                     : lookup;

    static constexpr auto name = Name;
    using value_type = value_type_at<Layout, index>;
    static constexpr value_type default_value = default_at<Layout, index>();
  };

  template<parameter_name Name>
  using value_type = typename parameter<Name>::value_type;

  static constexpr std::size_t base = Base;
  static constexpr std::size_t size = Layout.size();

  static_assert(Layout.names_unique(),
                "Duplicate or hash-colliding parameter names");

  static_assert(base + size <= Storage::capacity,
                "Layout does not fit in storage");

  constexpr explicit registry(Storage& storage) : storage_(storage) {}

  // Opaque ticket to one layout cell: lets callers build their own
  // per-value-type code paths (a table of parameters, a protocol adapter)
  // without re-specializing per name. Constructible only via ref<Name>() /
  // ref_at<I>(), so every instance went through the layout's compile-time
  // checks and no address outside the layout can be forged; the cell scheme
  // stays private.
  template<typename T>
  class parameter_ref {
    addr_type val_loc_;
    hash_type hash_;

    constexpr parameter_ref(addr_type val_loc, hash_type hash)
        : val_loc_(val_loc), hash_(hash)
    {
    }

    friend class registry;
  };

  template<std::size_t I>
  static consteval auto ref_at() -> parameter_ref<value_type_at<Layout, I>>
  {
    constexpr auto val_loc = base + Layout.offset_of(I) + sizeof(hash_type);
    return {val_loc, Layout.hash_at(I)};
  }

  template<parameter_name Name>
  static consteval auto ref() -> parameter_ref<value_type<Name>>
  {
    return ref_at<parameter<Name>::index>();
  }

  template<typename T>
  constexpr auto get(parameter_ref<T> p) -> std::expected<T, error>
  {
    return get_impl<T>(p.val_loc_, p.hash_);
  }

  template<typename T>
  constexpr auto set(parameter_ref<T> p, T const& val)
      -> std::expected<void, error>
  {
    return set_impl<T>(p.val_loc_, p.hash_, val);
  }

  template<parameter_name Name>
  constexpr auto get()
      -> std::expected<typename parameter<Name>::value_type, error>
  {
    return get(ref<Name>());
  }

  template<parameter_name Name>
  constexpr auto set(typename parameter<Name>::value_type const& val)
      -> std::expected<void, error>
  {
    return set(ref<Name>(), val);
  }

  template<parameter_name Name>
  constexpr auto reset()
  {
    return set(ref<Name>(), parameter<Name>::default_value);
  }

  constexpr auto reset_all() -> std::expected<void, error>
  {
    std::expected<void, error> r{};
    unroll<layout_type::count>([&]<std::size_t I>() {
      if (r) r = set(ref_at<I>(), default_at<Layout, I>());
    });
    return r;
  }

  auto erase() -> std::expected<void, error>
  {
    for (std::size_t off = base; off < base + size; ++off) {
      auto r = storage_.template write<std::byte>(
          typename Storage::addr_type(off),
          std::byte{0});
      if (!r) return r;
    }
    return {};
  }

private:
  template<typename T>
  [[gnu::noinline]] constexpr auto get_impl(addr_type val_loc, hash_type hash)
      -> std::expected<T, error>
  {
    auto const hash_loc = addr_type(val_loc - sizeof(hash_type));
    auto const crc_loc = addr_type(val_loc + sizeof(T));

    auto h = storage_.template read<hash_type>(hash_loc);
    if (!h) return std::unexpected(h.error());
    if (*h != hash) return std::unexpected(error::hash_mismatch);

    auto val = storage_.template read<T>(val_loc);
    if (!val) return std::unexpected(val.error());

    auto stored_crc = storage_.template read<crc_type>(crc_loc);
    if (!stored_crc) return std::unexpected(stored_crc.error());

    auto crc = crc_fn{}(hash, *val);
    if (*stored_crc != crc) return std::unexpected(error::crc_mismatch);

    return *val;
  }

  template<typename T>
  [[gnu::noinline]] constexpr auto set_impl(addr_type val_loc,
                                            hash_type hash,
                                            T const& val)
      -> std::expected<void, error>
  {
    auto const hash_loc = addr_type(val_loc - sizeof(hash_type));
    auto const crc_loc = addr_type(val_loc + sizeof(T));

    auto r1 = storage_.template write<hash_type>(hash_loc, hash);
    if (!r1) return r1;

    auto r2 = storage_.template write<T>(val_loc, val);
    if (!r2) return r2;

    auto crc = crc_fn{}(hash, val);
    return storage_.template write<crc_type>(crc_loc, crc);
  }
};

} // namespace nvm
} // namespace emb
