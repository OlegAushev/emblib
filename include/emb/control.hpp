#pragma once

#if __cplusplus >= 202300

#include <emb/core.hpp>

#include <concepts>
#include <variant>

namespace emb {
namespace control {

template<typename>
struct command_tag {};

template<typename T, typename Command>
concept command_source = requires(T& t) {
  { t.get(command_tag<Command>{}) } -> std::convertible_to<Command>;
};

template<typename T, typename Command>
concept command_sink = requires(T& sink, Command const& cmd) {
  { sink.accept(cmd) } -> std::same_as<void>;
};

template<
    typename Command,
    typename ScopedLock,
    typename Sink,
    typename... Sources>
class command_multiplexer {
public:
  using command_type = Command;
  using scoped_lock_type = ScopedLock;
  using sink_type = Sink;
  using source_type = std::variant<std::monostate, Sources const*...>;
private:
  sink_type& sink_;
  source_type source_;
public:
  constexpr explicit command_multiplexer(sink_type& obj)
    requires command_sink<sink_type, command_type>
      : sink_(obj) {}

  command_multiplexer(command_multiplexer const&) = delete;
  command_multiplexer(command_multiplexer&&) = delete;
  command_multiplexer& operator=(command_multiplexer const&) = delete;
  command_multiplexer& operator=(command_multiplexer&&) = delete;

  constexpr source_type source() const {
    [[maybe_unused]] scoped_lock_type lock{};
    return source_;
  }

  template<typename Device>
    requires command_source<Device, command_type> &&
             emb::same_as_any<Device, Sources...>
  constexpr void activate(Device const& device) {
    [[maybe_unused]] scoped_lock_type lock{};
    source_ = &device;
    sink_.accept(device.get(command_tag<command_type>{}));
  }

  constexpr void activate(source_type const& source) {
    [[maybe_unused]] scoped_lock_type lock{};
    source_ = source;
    auto visitor = [](auto const& s) -> command_type {
      if constexpr (std::same_as<
                        std::remove_cvref_t<decltype(s)>,
                        std::monostate>) {
        return command_type{};
      } else {
        return s->get(command_tag<command_type>{});
      }
    };
    sink_.accept(std::visit(visitor, source_));
  }

  constexpr void deactivate() {
    [[maybe_unused]] scoped_lock_type lock{};
    source_ = std::monostate{};
    sink_.accept(command_type{});
  }

  template<typename Device>
    requires command_source<Device, command_type> &&
             emb::same_as_any<Device, Sources...>
  constexpr bool is_active(Device const& device) const {
    [[maybe_unused]] scoped_lock_type lock{};
    return is_active_unlocked(device);
  }

  template<typename Device>
    requires command_source<Device, command_type> &&
             emb::same_as_any<Device, Sources...>
  constexpr bool try_send(Device const& sender, command_type const& cmd) {
    [[maybe_unused]] scoped_lock_type lock{};
    if (!is_active_unlocked(sender)) {
      return false;
    }
    sink_.accept(cmd);
    return true;
  }

private:
  template<typename Source>
    requires command_source<Source, command_type> &&
             emb::same_as_any<Source, Sources...>
  constexpr bool is_active_unlocked(Source const& source) const {
    auto ptr = std::get_if<Source const*>(&source_);
    return ptr && *ptr == &source;
  }
};

} // namespace control
} // namespace emb

#endif
