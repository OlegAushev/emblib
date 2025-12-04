#pragma once

#if __cplusplus >= 202300

#include <emb/mutex.hpp>

#include <concepts>
#include <variant>

namespace emb {
namespace control {

template<typename>
struct command_tag {};

template<typename T, typename Command>
concept command_source = requires(T const& t) {
  { t.get(command_tag<Command>{}) } -> std::convertible_to<Command>;
};

template<typename T, typename Command>
concept command_sink = requires(T& sink, Command const& cmd) {
  { sink.accept(cmd) } -> std::same_as<void>;
};

template<typename Command, typename Mutex, typename Sink, typename... Sources>
class command_multiplexer {
public:
  using command_type = Command;
  using mutex_type = Mutex;
  using sink_type = Sink;
  using source_variant = std::variant<std::monostate, Sources const*...>;
private:
  sink_type& sink_;
  source_variant source_;
  mutex_type mutable mutex_;

public:
  constexpr explicit command_multiplexer(sink_type& obj)
    requires command_sink<sink_type, command_type>
      : sink_(obj) {}

  command_multiplexer(command_multiplexer const&) = delete;
  command_multiplexer(command_multiplexer&&) = delete;
  command_multiplexer& operator=(command_multiplexer const&) = delete;
  command_multiplexer& operator=(command_multiplexer&&) = delete;

  template<typename Device>
    requires command_source<Device, command_type> &&
             emb::one_of<Device, Sources...>
  constexpr void activate(Device const& device) {
    emb::lock_guard<mutex_type> lock(mutex_);
    source_ = &device;
    sink_.accept(device.get(command_tag<command_type>{}));
  }

  constexpr void deactivate() {
    emb::lock_guard<mutex_type> lock(mutex_);
    source_ = std::monostate{};
  }

  template<typename Device>
    requires command_source<Device, command_type> &&
             emb::one_of<Device, Sources...>
  constexpr bool is_active(Device const& device) const {
    emb::lock_guard<mutex_type> lock(mutex_);
    return is_active_unlocked(device);
  }

  template<typename Device>
    requires command_source<Device, command_type> &&
             emb::one_of<Device, Sources...>
  constexpr bool try_send(Device const& sender, command_type const& cmd) {
    emb::lock_guard<mutex_type> lock(mutex_);
    if (!is_active_unlocked(sender)) {
      return false;
    }
    sink_.accept(cmd);
    return true;
  }

private:
  template<typename Source>
    requires command_source<Source, command_type> &&
             emb::one_of<Source, Sources...>
  constexpr bool is_active_unlocked(Source const& source) const {
    auto ptr = std::get_if<Source const*>(&source_);
    return ptr && *ptr == &source;
  }
};

} // namespace control
} // namespace emb

#endif
