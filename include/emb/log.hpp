#pragma once

#include <emb/core.hpp>
#include <emb/queue.hpp>

#include <array>
#include <bitset>
#include <optional>
#include <utility>
#include <variant>

namespace emb {
namespace log {

enum class level : uint8_t { info, notice, warning, error, critical };

inline constexpr size_t num_levels = 5;

inline constexpr std::array<level, num_levels> levels_list =
    {level::info, level::notice, level::warning, level::error, level::critical};

template<size_t Capacity, typename LockGuard, typename... Events>
class basic_logger {
private:
  using flags_container_inner_type = std::array<std::bitset<32>, num_levels>;

  using flags_container_type =
      std::array<flags_container_inner_type, sizeof...(Events)>;

  template<typename Event>
  static constexpr size_t group_index() {
    return type_index_v<Event, Events...>;
  }

  static constexpr size_t level_index(level lvl) {
    return static_cast<size_t>(std::to_underlying(lvl));
  }

  template<typename Event>
  static constexpr size_t event_index(Event event) {
    return static_cast<size_t>(std::to_underlying(event));
  }
public:
  using event_type = std::variant<Events...>;

  using payload_type =
      std::variant<std::monostate, bool, uint32_t, int32_t, float>;

  struct logged_data_type {
    level lvl;
    event_type event;
    payload_type payload;
  };
private:
  flags_container_type flags_{};
  queue<logged_data_type, Capacity> data_;
public:
  template<typename Event>
    requires same_as_any<Event, Events...>
  constexpr void log(level lvl, Event event, payload_type payload = {}) {
    [[maybe_unused]] LockGuard lock_guard;
    flags_[group_index<Event>()][level_index(lvl)].set(event_index(event));
    if (!data_.full()) {
      data_.push({lvl, event, payload});
    }
  }

  template<typename Event>
    requires same_as_any<Event, Events...>
  constexpr void clear(level lvl, Event event) {
    [[maybe_unused]] LockGuard lock_guard;
    flags_[group_index<Event>()][level_index(lvl)].reset(event_index(event));
  }

  template<typename Event>
    requires same_as_any<Event, Events...>
  constexpr void info(Event event, payload_type payload = {}) {
    log(level::info, event, payload);
  }

  template<typename Event>
    requires same_as_any<Event, Events...>
  constexpr void notice(Event event, payload_type payload = {}) {
    log(level::notice, event, payload);
  }

  template<typename Event>
    requires same_as_any<Event, Events...>
  constexpr void warning(Event event, payload_type payload = {}) {
    log(level::warning, event, payload);
  }

  template<typename Event>
    requires same_as_any<Event, Events...>
  constexpr void error(Event event, payload_type payload = {}) {
    log(level::error, event, payload);
  }

  template<typename Event>
    requires same_as_any<Event, Events...>
  constexpr void critical(Event event, payload_type payload = {}) {
    log(level::critical, event, payload);
  }

  template<typename Event>
    requires same_as_any<Event, Events...>
  constexpr std::optional<level> test(Event event) {
    for (auto lvl = levels_list.rbegin(); lvl != levels_list.rend(); ++lvl) {
      if (flags_[type_index_v<Event, Events...>][level_index(*lvl)]
                [event_index(event)]) {
        return *lvl;
      }
    }
    return std::nullopt;
  }

  constexpr std::optional<logged_data_type> pop_message() {
    [[maybe_unused]] LockGuard lock_guard;
    if (data_.empty()) {
      return std::nullopt;
    }
    logged_data_type ret = data_.front();
    data_.pop();
    if (ret.lvl == level::info) {
      auto group_idx = ret.event.index();
      auto event_idx = std::visit(
          [](auto ev) { return event_index(ev); },
          ret.event
      );
      flags_[group_idx][level_index(ret.lvl)].reset(event_idx);
    }
    return ret;
  }
};

} // namespace log
} // namespace emb
