#pragma once

#include <algorithm>
#include <array>
#include <bit>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <expected>

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

template<size_t N>
struct parameter_name {
  char data[N]{};

  constexpr parameter_name(char const (&str)[N]) {
    std::copy_n(str, N, data);
  }

  constexpr size_t size() const {
    return N - 1;
  }

  template<size_t M>
  constexpr bool operator==(parameter_name<M> const& other) const {
    if (N != M) return false;
    for (size_t i = 0; i < N; ++i) {
      if (data[i] != other.data[i]) return false;
    }
    return true;
  }
};

template<size_t N>
parameter_name(char const (&)[N]) -> parameter_name<N>;

// -- Hash function objects --

struct fnv1a_32 {
  using type = uint32_t;

  template<size_t N>
  static consteval type operator()(parameter_name<N> const& s) {
    type h = 0x811C9DC5u;
    for (size_t i = 0; i < N - 1; ++i) {
      h ^= static_cast<uint8_t>(s.data[i]);
      h *= 0x01000193u;
    }
    return h;
  }

  static constexpr type operator()(const char* s) {
    type h = 0x811C9DC5u;
    for (; *s; ++s) {
      h ^= static_cast<uint8_t>(*s);
      h *= 0x01000193u;
    }
    return h;
  }
};

// -- CRC function objects --

struct crc32 {
  using type = uint32_t;

  template<typename Hash, typename Value>
  static constexpr type operator()(Hash const& h, Value const& val) {
    auto hb = std::bit_cast<std::array<uint8_t, sizeof(Hash)>>(h);
    auto vb = std::bit_cast<std::array<uint8_t, sizeof(Value)>>(val);
    type crc = 0xFFFFFFFFu;
    for (auto b : hb) crc = update(crc, b);
    for (auto b : vb) crc = update(crc, b);
    return crc ^ 0xFFFFFFFFu;
  }

private:
  static constexpr type update(type crc, uint8_t b) {
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
                   && requires(F f, uint32_t h, uint32_t v) {
                        { f(h, v) } -> std::same_as<typename F::type>;
                      };

// -- Parameter --

template<parameter_name Name>
struct parameter {
  static constexpr bool exists = false;
};

template<some_hash_fn HashFn = fnv1a_32>
struct layout_entry {
  typename HashFn::type hash;
  size_t size;
};

template<
    parameter_name Name,
    typename T,
    T Default,
    some_hash_fn HashFn = fnv1a_32>
struct parameter_traits {
  using value_type = T;
  using hash_type = typename HashFn::type;
  static constexpr bool exists = true;
  static constexpr auto name = parameter_name(Name);
  static constexpr typename HashFn::type hash = HashFn{}(name);
  static constexpr size_t size = sizeof(T);
  static constexpr value_type default_value = Default;
  static constexpr layout_entry<HashFn> entry = {hash, size};
};

// -- Layout --

template<size_t N, some_hash_fn HashFn = fnv1a_32, some_crc_fn CrcFn = crc32>
struct layout {
  using hash_fn = HashFn;
  using crc_fn = CrcFn;
  using hash_type = typename HashFn::type;
  using crc_type = typename CrcFn::type;
  static constexpr size_t overhead = sizeof(hash_type) + sizeof(crc_type);

  size_t base = 0;
  layout_entry<HashFn> entries[N] = {};

  consteval size_t offset_of(hash_type key) const {
    size_t off = base;
    for (size_t i = 0; i < N; ++i) {
      if (entries[i].hash == key) return off;
      off += entries[i].size + overhead;
    }
    return SIZE_MAX;
  }

  consteval size_t size() const {
    size_t s = 0;
    for (size_t i = 0; i < N; ++i)
      s += entries[i].size + overhead;
    return s;
  }

  consteval bool names_unique() const {
    for (size_t i = 0; i < N; ++i)
      for (size_t j = i + 1; j < N; ++j)
        if (entries[i].hash == entries[j].hash) return false;
    return true;
  }
};

template<size_t N, some_hash_fn HashFn>
layout(size_t, layout_entry<HashFn> const (&)[N]) -> layout<N, HashFn>;

// -- Storage backend concept --

template<typename T>
concept some_storage = requires {
      typename T::addr_type;
      { T::capacity } -> std::convertible_to<size_t>;
    }
    && requires(T& s, typename T::addr_type addr) {
      { s.template read<uint32_t>(addr) }
          -> std::same_as<std::expected<uint32_t, nvm::error>>;
      { s.template write<uint32_t>(addr, uint32_t{}) }
          -> std::same_as<std::expected<void, nvm::error>>;
    };

// -- Registry --

template<some_storage Storage, auto& Layout>
class registry {
  Storage& storage_;

  using layout_type = std::remove_cvref_t<decltype(Layout)>;
  using hash_type = typename layout_type::hash_type;
  using crc_type = typename layout_type::crc_type;
  using crc_fn = typename layout_type::crc_fn;

public:
  static constexpr size_t size = Layout.size();

  static_assert(
      Layout.names_unique(),
      "Duplicate or hash-colliding parameter names"
  );
  static_assert(
      Layout.base + Layout.size() <= Storage::capacity,
      "Layout does not fit in storage"
  );

  constexpr explicit registry(Storage& storage) : storage_(storage) {}

  template<parameter_name Name>
  constexpr auto get()
      -> std::expected<typename parameter<Name>::value_type, error> {
    using P = parameter<Name>;
    static_assert(P::exists, "Unknown parameter");
    constexpr auto offset = Layout.offset_of(P::hash);
    static_assert(offset != SIZE_MAX, "Parameter not in layout");

    constexpr auto hash_loc = offset;
    auto h = storage_.template read<hash_type>(hash_loc);
    if (!h) return std::unexpected(h.error());
    if (*h != P::hash) return std::unexpected(error::hash_mismatch);

    constexpr auto val_loc = offset + sizeof(hash_type);
    auto val = storage_.template read<typename P::value_type>(val_loc);
    if (!val) return std::unexpected(val.error());

    constexpr auto crc_loc = offset + sizeof(hash_type) + P::size;
    auto stored_crc = storage_.template read<crc_type>(crc_loc);
    if (!stored_crc) return std::unexpected(stored_crc.error());

    auto crc = crc_fn{}(P::hash, *val);
    if (*stored_crc != crc) return std::unexpected(error::crc_mismatch);

    return *val;
  }

  template<parameter_name Name>
  constexpr auto set(typename parameter<Name>::value_type const& val)
      -> std::expected<void, error> {
    using P = parameter<Name>;
    static_assert(P::exists, "Unknown parameter");
    constexpr auto offset = Layout.offset_of(P::hash);
    static_assert(offset != SIZE_MAX, "Parameter not in layout");

    constexpr auto hash_loc = offset;
    auto r1 = storage_.template write<hash_type>(hash_loc, P::hash);
    if (!r1) return r1;

    constexpr auto val_loc = offset + sizeof(hash_type);
    auto r2 = storage_.template write<typename P::value_type>(val_loc, val);
    if (!r2) return r2;

    auto crc = crc_fn{}(P::hash, val);
    constexpr auto crc_loc = offset + sizeof(hash_type) + P::size;
    return storage_.template write<crc_type>(crc_loc, crc);
  }

  template<parameter_name Name>
  constexpr auto reset() {
    using P = parameter<Name>;
    return set<Name>(P::default_value);
  }

  auto erase() -> std::expected<void, error> {
    for (size_t off = Layout.base; off < Layout.base + size; ++off) {
      auto r = storage_.template write<std::byte>(
          typename Storage::addr_type(off),
          std::byte{0}
      );
      if (!r) return r;
    }
    return {};
  }
};

} // namespace nvm
} // namespace emb
