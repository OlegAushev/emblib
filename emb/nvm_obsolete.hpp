#pragma once

// The first of three takes on NVM parameters, kept for reference: a pack of
// parameter<Name, Default> types. It was superseded by a constexpr array of
// {name, default} entries over a typelist, and that in turn by <emb/settings>,
// where the schema carries bounds, groups and apply policies and the medium
// holds whole records rather than fixed cells.
//
// Defines its own emb::nvm::error, from the days when that vocabulary was
// the library's rather than each driver's.

#include <emb/meta.hpp>

#include <algorithm>
#include <array>
#include <bit>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <type_traits>

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

template<std::size_t N>
struct parameter_name {
  char data[N]{};

  constexpr parameter_name(char const (&str)[N]) {
    std::copy_n(str, N, data);
  }

  constexpr std::size_t size() const {
    return N - 1;
  }

  template<std::size_t M>
  constexpr bool operator==(parameter_name<M> const& other) const {
    if (N != M) return false;
    for (auto i = 0uz; i < N; ++i) {
      if (data[i] != other.data[i]) return false;
    }
    return true;
  }
};

template<std::size_t N>
parameter_name(char const (&)[N]) -> parameter_name<N>;

// -- Hash function objects --

struct fnv1a_32 {
  using type = std::uint32_t;

  template<std::size_t N>
  static consteval type operator()(parameter_name<N> const& s) {
    type h = 0x811C9DC5u;
    for (auto i = 0uz; i < N - 1; ++i) {
      h ^= static_cast<std::uint8_t>(s.data[i]);
      h *= 0x01000193u;
    }
    return h;
  }

  static constexpr type operator()(char const* s) {
    type h = 0x811C9DC5u;
    for (; *s; ++s) {
      h ^= static_cast<std::uint8_t>(*s);
      h *= 0x01000193u;
    }
    return h;
  }
};

// -- CRC function objects --

struct crc32 {
  using type = std::uint32_t;

  template<typename Hash, typename Value>
  static constexpr type operator()(Hash const& h, Value const& val) {
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
  static constexpr type update(type crc, std::uint8_t b) {
    crc ^= b;
    for (int i = 0; i < 8; ++i)
      crc = (crc >> 1) ^ (0xEDB88320u & (-(crc & 1u)));
    return crc;
  }
};

// -- Concepts --

template<typename F>
concept some_hash_fn = requires { typename F::type; }
                    && requires(F f, char const* s) {
                         { f(s) } -> std::same_as<typename F::type>;
                       };

template<typename F>
concept some_crc_fn = requires { typename F::type; }
                   && requires(F f, std::uint32_t h, std::uint32_t v) {
                        { f(h, v) } -> std::same_as<typename F::type>;
                      };

// -- Parameter --

template<parameter_name Name, auto Default>
struct parameter {
  using value_type = std::remove_cv_t<decltype(Default)>;
  static constexpr auto name = Name;
  static constexpr value_type default_value = Default;
};

template<typename P>
concept some_parameter = requires {
  typename P::value_type;
  requires std::same_as<
      std::remove_cv_t<decltype(P::default_value)>,
      typename P::value_type>;
  P::name.size();
};

// -- Layout --

// Never defined: calling it from a consteval context fails constant
// evaluation with this name in the diagnostic.
void parameter_index_out_of_range();

template<some_hash_fn HashFn, some_crc_fn CrcFn, some_parameter... Params>
struct basic_layout {
  using hash_fn = HashFn;
  using crc_fn = CrcFn;
  using hash_type = typename HashFn::type;
  using crc_type = typename CrcFn::type;
  static constexpr std::size_t count = sizeof...(Params);
  static constexpr std::size_t overhead = sizeof(hash_type) + sizeof(crc_type);

  static constexpr std::array<hash_type, count> hashes{
      HashFn{}(Params::name)...
  };

  static constexpr std::array<std::size_t, count> sizes{
      sizeof(typename Params::value_type)...
  };

  std::size_t base = 0;

  template<parameter_name Name>
  static consteval std::size_t index_of() {
    constexpr auto key = HashFn{}(Name);
    for (auto i = 0uz; i < count; ++i) {
      if (hashes[i] == key) return i;
    }
    return SIZE_MAX;
  }

  template<parameter_name Name>
  static consteval std::size_t checked_index_of() {
    constexpr std::size_t i = index_of<Name>();
    static_assert(i != SIZE_MAX, "Unknown parameter");
    if constexpr (i != SIZE_MAX) {
      // index_of matches by hash only; reject a foreign name that merely
      // collides with a layout entry's hash.
      static_assert(
          parameter_at<i>::name == Name,
          "Parameter name hash collision"
      );
    }
    return i;
  }

  template<parameter_name Name>
  using parameter_of = nth_type_t<checked_index_of<Name>(), Params...>;

  template<std::size_t I>
  using parameter_at = nth_type_t<I, Params...>;

  consteval std::size_t offset_of(std::size_t index) const {
    if (index >= count) parameter_index_out_of_range();
    std::size_t off = base;
    for (auto i = 0uz; i < index; ++i)
      off += sizes[i] + overhead;
    return off;
  }

  template<parameter_name Name>
  consteval std::size_t offset_of() const {
    return offset_of(checked_index_of<Name>());
  }

  static consteval std::size_t size() {
    std::size_t s = 0;
    for (auto i = 0uz; i < count; ++i)
      s += sizes[i] + overhead;
    return s;
  }

  static consteval bool names_unique() {
    for (auto i = 0uz; i < count; ++i)
      for (auto j = i + 1; j < count; ++j)
        if (hashes[i] == hashes[j]) return false;
    return true;
  }
};

template<some_parameter... Params>
using layout = basic_layout<fnv1a_32, crc32, Params...>;

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

template<some_storage Storage, auto& Layout>
class registry {
  Storage& storage_;

  using layout_type = std::remove_cvref_t<decltype(Layout)>;
  using hash_type = typename layout_type::hash_type;
  using crc_type = typename layout_type::crc_type;
  using crc_fn = typename layout_type::crc_fn;
  using addr_type = typename Storage::addr_type;

  template<parameter_name Name>
  static constexpr std::size_t index =
      layout_type::template checked_index_of<Name>();

public:
  // Member struct rather than a plain alias chain: alias templates are
  // transparent to name mangling, so spelling the dependent value type
  // through them in get/set signatures would expand the whole parameter pack
  // into every mangled symbol (~150 bytes per parameter). A nested class
  // mangles by name, keeping symbols O(name), not O(layout).
  template<parameter_name Name>
  struct traits {
    using parameter = typename layout_type::template parameter_of<Name>;
    using value_type = typename parameter::value_type;
  };

  template<parameter_name Name>
  using parameter = typename traits<Name>::parameter;

  template<parameter_name Name>
  using value_type = typename traits<Name>::value_type;

  static constexpr std::size_t size = Layout.size();

  static_assert(
      Layout.names_unique(),
      "Duplicate or hash-colliding parameter names"
  );
  static_assert(
      Layout.base + Layout.size() <= Storage::capacity,
      "Layout does not fit in storage"
  );

  constexpr explicit registry(Storage& storage) : storage_(storage) {}

  // Opaque ticket to one layout cell: lets callers build their own
  // per-value-type code paths (a table of parameters, a protocol adapter)
  // without re-specializing per name. Constructible only via ref<Name>(), so
  // every instance went through the layout's compile-time name checks and no
  // address outside the layout can be forged; the cell scheme stays private.
  template<typename T>
  class parameter_ref {
    addr_type hash_loc_;
    addr_type val_loc_;
    addr_type crc_loc_;
    hash_type hash_;

    constexpr parameter_ref(
        addr_type hash_loc, addr_type val_loc, addr_type crc_loc,
        hash_type hash
    )
        : hash_loc_(hash_loc), val_loc_(val_loc), crc_loc_(crc_loc),
          hash_(hash) {}

    friend class registry;
  };

  template<parameter_name Name>
  static consteval auto ref() -> parameter_ref<value_type<Name>> {
    constexpr auto hash_loc = Layout.offset_of(index<Name>);
    constexpr auto val_loc = hash_loc + sizeof(hash_type);
    constexpr auto crc_loc = val_loc + sizeof(value_type<Name>);
    return {hash_loc, val_loc, crc_loc, layout_type::hashes[index<Name>]};
  }

  template<typename T>
  constexpr auto get(parameter_ref<T> p) -> std::expected<T, error> {
    return get_impl<T>(p.hash_loc_, p.val_loc_, p.crc_loc_, p.hash_);
  }

  template<typename T>
  constexpr auto set(parameter_ref<T> p, T const& val)
      -> std::expected<void, error> {
    return set_impl<T>(p.hash_loc_, p.val_loc_, p.crc_loc_, p.hash_, val);
  }

  template<parameter_name Name>
  constexpr auto get()
      -> std::expected<typename traits<Name>::value_type, error> {
    return get(ref<Name>());
  }

  template<parameter_name Name>
  constexpr auto set(typename traits<Name>::value_type const& val)
      -> std::expected<void, error> {
    return set(ref<Name>(), val);
  }

  template<parameter_name Name>
  constexpr auto reset() {
    return set<Name>(parameter<Name>::default_value);
  }

  // Iterates by index instead of expanding the parameter pack in a lambda:
  // a lambda specialized on Params... would emit one more pack-sized symbol.
  constexpr auto reset_all() -> std::expected<void, error> {
    std::expected<void, error> r{};
    unroll<layout_type::count>([&]<std::size_t I>() {
      using P = typename layout_type::template parameter_at<I>;
      if (r) r = this->template reset<P::name>();
    });
    return r;
  }

  auto erase() -> std::expected<void, error> {
    for (std::size_t off = Layout.base; off < Layout.base + size; ++off) {
      auto r = storage_.template write<std::byte>(
          typename Storage::addr_type(off),
          std::byte{0}
      );
      if (!r) return r;
    }
    return {};
  }

private:
  template<typename T>
  [[gnu::noinline]] constexpr auto get_impl(
      addr_type hash_loc,
      addr_type val_loc,
      addr_type crc_loc,
      hash_type hash
  ) -> std::expected<T, error> {
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
  [[gnu::noinline]] constexpr auto set_impl(
      addr_type hash_loc,
      addr_type val_loc,
      addr_type crc_loc,
      hash_type hash,
      T const& val
  ) -> std::expected<void, error> {
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
