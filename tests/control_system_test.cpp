#ifdef __cpp_constexpr

#include <emb/control_system.hpp>
#include <emb/units.hpp>

namespace emb {
namespace internal {
namespace tests {

class dummy_mutex {
public:
  constexpr void lock() {}
  constexpr void unlock() {}
};

class button;
class knob;
class throttle;
class drive;

using startmux_type = emb::ctlsys::
    control_multiplexer<drive, bool, dummy_mutex, button, throttle>;
using speedmux_type =
    emb::ctlsys::control_multiplexer<drive, float, dummy_mutex, knob, throttle>;

class drive {
private:
  bool should_start_ = false;
  float speed_ref_ = 0.0f;
public:
  constexpr void update_setpoint(bool should_start) {
    should_start_ = should_start;
  }

  constexpr void update_setpoint(float ref) {
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
  startmux_type& startmux_;
public:
  constexpr explicit button(startmux_type& startmux) : startmux_(startmux) {}

  constexpr bool setpoint(emb::ctlsys::control_variable_tag<bool>) const {
    return pressed_;
  }

  constexpr void toggle() {
    pressed_ = !pressed_;
    startmux_.try_update_setpoint(*this, pressed_);
  }
};

class knob {
private:
  float val_ = 0.0f;
  speedmux_type& speedmux_;
public:
  constexpr explicit knob(speedmux_type& speedmux) : speedmux_(speedmux) {}

  constexpr float setpoint(emb::ctlsys::control_variable_tag<float>) const {
    return val_;
  }

  constexpr void set(float val) {
    val_ = val;
    speedmux_.try_update_setpoint(*this, val_);
  }
};

class throttle {
private:
  bool should_start_ = false;
  float speed_ref_ = 0.0f;
  startmux_type& startmux_;
  speedmux_type& speedmux_;
public:
  constexpr throttle(startmux_type& startmux, speedmux_type& speedmux)
      : startmux_(startmux), speedmux_(speedmux) {}

  constexpr bool setpoint(emb::ctlsys::control_variable_tag<bool>) const {
    return should_start_;
  }

  constexpr float setpoint(emb::ctlsys::control_variable_tag<float>) const {
    return speed_ref_;
  }

  constexpr void set(bool should_start, float val) {
    should_start_ = should_start;
    speed_ref_ = val;
    startmux_.try_update_setpoint(*this, should_start_);
    speedmux_.try_update_setpoint(*this, speed_ref_);
  }
};

constexpr bool test_control_device() {
  drive drv;
  startmux_type startmux(drv);
  speedmux_type speedmux(drv);

  button btn(startmux);
  knob kb(speedmux);
  throttle trtl(startmux, speedmux);

  assert(!startmux.is_active(btn));
  assert(!startmux.is_active(trtl));

  assert(!speedmux.is_active(kb));
  assert(!speedmux.is_active(trtl));

  btn.toggle();
  assert(!drv.should_start());

  startmux.activate(btn);
  assert(drv.should_start());

  assert(drv.speed_ref() == 0.0f);
  kb.set(42.0f);
  speedmux.activate(kb);
  assert(drv.speed_ref() == 42.0f);

  speedmux.activate(trtl);
  trtl.set(false, 1.0f);
  assert(drv.should_start());
  assert(drv.speed_ref() == 1.0f);

  startmux.activate(trtl);
  assert(!drv.should_start());

  return true;
}

static_assert(test_control_device());

} // namespace tests
} // namespace internal
} // namespace emb

#endif
