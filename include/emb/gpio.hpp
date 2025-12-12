#pragma once

#include <emb/core.hpp>
#include <emb/scopedenum.hpp>

namespace emb {
namespace gpio {

#if __cplusplus >= 201100

enum class level : int32_t {
  low = 0,
  high = 1
};

constexpr level operator!(level lvl) {
  if (lvl == level::low) {
    return level::high;
  } else {
    return level::low;
  }
}

enum class state : int32_t {
  inactive = 0,
  active = 1
};

#ifdef __cpp_concepts

template<typename T>
concept input = requires(const T t) {
    { t.read() } -> std::same_as<state>;
    { t.read_level() } -> std::same_as<level>;
};

template<typename T>
concept output = requires(T t, const T ct, state s, level lvl) {
    { ct.read() } -> std::same_as<state>;
    { t.set(s) } -> std::same_as<void>;
    { t.set() } -> std::same_as<void>;
    { t.reset() } -> std::same_as<void>;
    { t.toggle() } -> std::same_as<void>;
    { ct.read_level() } -> std::same_as<level>;
    { t.set_level(lvl) } -> std::same_as<void>;
};

#else

class input {
public:
  input() = default;
  virtual ~input() = default;

  virtual state read() const = 0;
  virtual level read_level() const = 0;
};

class output {
public:
  output() = default;
  virtual ~output() = default;

  virtual state read() const = 0;
  virtual void set(state s = state::active) = 0;
  virtual void reset() = 0;
  virtual void toggle() = 0;
  virtual level read_level() const = 0;
  virtual void set_level(level lvl) = 0;
};

#endif

#else
// clang-format off
SCOPED_ENUM_UT_DECLARE_BEGIN(active_state, unsigned int) {
  low = 0,
  high = 1
} SCOPED_ENUM_DECLARE_END(active_state);

SCOPED_ENUM_UT_DECLARE_BEGIN(state, unsigned int) {
  inactive = 0,
  active = 1
} SCOPED_ENUM_DECLARE_END(state);
// clang-format on

class input {
public:
  input() {}

  virtual ~input() {}

  virtual state read() const = 0;
  virtual unsigned int read_level() const = 0;
};

class output {
public:
  output() {}

  virtual ~output() {}

  virtual state read() const = 0;
  virtual void set(state s = state::active) = 0;
  virtual void reset() = 0;
  virtual void toggle() = 0;
  virtual unsigned int read_level() const = 0;
  virtual void set_level(unsigned int level) = 0;
};

#endif

} // namespace gpio
} // namespace emb
