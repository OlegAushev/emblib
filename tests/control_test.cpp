#ifdef __cpp_constexpr

#include <emb/control.hpp>
#include <emb/units.hpp>

namespace emb {
namespace internal {
namespace tests {

class dummy_lock {
public:
  constexpr void lock() {}
  constexpr void unlock() {}
};

class button;
class knob;
class throttle;
class drive;

using startmux_type = emb::control::
    command_multiplexer<bool, dummy_lock, drive, button, throttle>;
using speedmux_type =
    emb::control::command_multiplexer<float, dummy_lock, drive, knob, throttle>;

class drive {
private:
  bool should_start_ = false;
  float speed_ref_ = 0.0f;
public:
  constexpr void accept(bool should_start) {
    should_start_ = should_start;
  }

  constexpr void accept(float ref) {
    speed_ref_ = ref;
  }

  constexpr bool should_start() const {
    return should_start_;
  }

  constexpr float speed_ref() const {
    return speed_ref_;
  }
};

class button {
private:
  bool pressed_ = false;
public:
  constexpr bool get(emb::control::command_tag<bool>) const {
    return pressed_;
  }

  constexpr void toggle() {
    pressed_ = !pressed_;
  }
};

class knob {
private:
  float val_ = 0.0f;
public:
  constexpr float get(emb::control::command_tag<float>) const {
    return val_;
  }

  constexpr void set(float val) {
    val_ = val;
  }
};

class throttle {
private:
  bool should_start_ = false;
  float speed_ref_ = 0.0f;
public:
  constexpr bool get(emb::control::command_tag<bool>) const {
    return should_start_;
  }

  constexpr float get(emb::control::command_tag<float>) const {
    return speed_ref_;
  }

  constexpr void set(bool should_start, float val) {
    should_start_ = should_start;
    speed_ref_ = val;
  }
};

constexpr bool test_control_device() {
  drive drv;
  startmux_type startmux(drv);
  speedmux_type speedmux(drv);

  button btn;
  knob kb;
  throttle trtl;

  assert(!startmux.is_active(btn));
  assert(!startmux.is_active(trtl));

  assert(!speedmux.is_active(kb));
  assert(!speedmux.is_active(trtl));

  btn.toggle();
  startmux.poll();
  assert(!drv.should_start());

  startmux.activate(btn);
  assert(drv.should_start());

  assert(drv.speed_ref() == 0.0f);
  kb.set(42.0f);
  speedmux.activate(kb);
  assert(drv.speed_ref() == 42.0f);

  speedmux.activate(trtl);
  trtl.set(false, 1.0f);
  speedmux.poll();
  assert(drv.should_start());
  assert(drv.speed_ref() == 1.0f);

  startmux.activate(trtl);
  assert(!drv.should_start());

  auto start_control = startmux.source();
  startmux.activate(btn);
  assert(drv.should_start());

  startmux.activate(start_control);
  assert(!drv.should_start());

  auto speed_control = speedmux.source();
  speedmux.deactivate();
  assert(drv.speed_ref() == 0.0f);
  speedmux.activate(speed_control);
  assert(drv.speed_ref() == 1.0f);
  speed_control = std::monostate{};
  speedmux.activate(speed_control);
  assert(drv.speed_ref() == 0.0f);

  return true;
}

static_assert(test_control_device());

} // namespace tests
} // namespace internal
} // namespace emb

#endif
