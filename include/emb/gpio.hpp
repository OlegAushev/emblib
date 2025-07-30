#pragma once

#include <emb/core.hpp>
#include <emb/noncopyable.hpp>
#include <emb/scopedenum.hpp>

namespace emb {
namespace gpio {

#if __cplusplus >= 201100

enum class level : int {
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

enum class state : int {
  inactive = 0,
  active = 1
};

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
