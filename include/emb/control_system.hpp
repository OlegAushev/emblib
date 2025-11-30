#pragma once

#if __cplusplus >= 202300

#include <emb/mutex.hpp>
#include <emb/units.hpp>

#include <concepts>
#include <variant>

namespace emb {
namespace ctlsys {

template<typename>
struct control_variable_tag {};

template<typename T, typename Variable>
concept control_device = requires(T const& device) {
  {
    device.setpoint(control_variable_tag<Variable>{})
  } -> std::convertible_to<Variable>;
};

template<typename T, typename Variable>
concept control_object = requires(T& object, Variable const& ref) {
  { object.update_setpoint(ref) } -> std::same_as<void>;
};

template<
    typename Object,
    typename Variable,
    typename Mutex,
    typename... ControlDevices>
class control_multiplexer {
public:
  using object_type = Object;
  using mutex_type = Mutex;
  using variable_type = Variable;
  using device_variant = std::variant<std::monostate, ControlDevices const*...>;
private:
  object_type& ctlobj_;
  device_variant ctldev_;
  mutex_type mutable mutex_;

public:
  constexpr explicit control_multiplexer(object_type& obj)
    requires control_object<object_type, variable_type>
      : ctlobj_(obj) {}

  control_multiplexer(control_multiplexer const&) = delete;
  control_multiplexer(control_multiplexer&&) = delete;
  control_multiplexer& operator=(control_multiplexer const&) = delete;
  control_multiplexer& operator=(control_multiplexer&&) = delete;

  template<typename Device>
    requires control_device<Device, variable_type> &&
             emb::one_of<Device, ControlDevices...>
  constexpr void activate(Device const& device) {
    emb::lock_guard<mutex_type> lock(mutex_);
    ctldev_ = &device;
    ctlobj_.update_setpoint(
        device.setpoint(control_variable_tag<variable_type>{})
    );
  }

  constexpr void deactivate() {
    emb::lock_guard<mutex_type> lock(mutex_);
    ctldev_ = std::monostate{};
  }

  template<typename Device>
    requires control_device<Device, variable_type> &&
             emb::one_of<Device, ControlDevices...>
  constexpr bool is_active(Device const& device) const {
    emb::lock_guard<mutex_type> lock(mutex_);
    return is_active_unlocked(device);
  }

  template<typename Device>
    requires control_device<Device, variable_type> &&
             emb::one_of<Device, ControlDevices...>
  constexpr bool try_update_setpoint(
      Device const& sender,
      Variable const& ref
  ) {
    emb::lock_guard<mutex_type> lock(mutex_);
    if (!is_active_unlocked(sender)) {
      return false;
    }
    ctlobj_.update_setpoint(ref);
    return true;
  }

private:
  template<typename Device>
    requires control_device<Device, variable_type> &&
             emb::one_of<Device, ControlDevices...>
  constexpr bool is_active_unlocked(Device const& device) const {
    auto ptr = std::get_if<Device const*>(&ctldev_);
    return ptr && *ptr == &device;
  }
};

} // namespace ctlsys
} // namespace emb

#endif
