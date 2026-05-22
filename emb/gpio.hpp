#pragma once

#include <concepts>
#include <cstdint>

namespace emb {
namespace gpio {

enum class level : std::int32_t { low = 0, high = 1 };

enum class polarity : std::int32_t { active_low = 0, active_high = 1 };

enum class state : std::int32_t { inactive = 0, active = 1 };

constexpr level operator!(level lvl) {
  return lvl == level::low ? level::high : level::low;
}

constexpr state to_state(level l, polarity p) {
  bool const active = (l == level::high) == (p == polarity::active_high);
  return active ? state::active : state::inactive;
}

constexpr level to_level(state s, polarity p) {
  bool const high = (s == state::active) == (p == polarity::active_high);
  return high ? level::high : level::low;
}

template<typename T>
concept input = requires(T const t) {
  { t.read() } -> std::same_as<state>;
  { t.read_level() } -> std::same_as<level>;
};

template<typename T>
concept output = requires(T t, T const ct, state s, level lvl) {
  { ct.read() } -> std::same_as<state>;
  { t.set(s) } -> std::same_as<void>;
  { t.set() } -> std::same_as<void>;
  { t.reset() } -> std::same_as<void>;
  { t.toggle() } -> std::same_as<void>;
  { ct.read_level() } -> std::same_as<level>;
  { t.set_level(lvl) } -> std::same_as<void>;
};

} // namespace gpio
} // namespace emb
