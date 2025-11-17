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
inline constexpr size_t num_flag_levels = 4;

template<size_t Capacity, typename... Events>
class basic_logger {
private:
  using flags_container_inner_type =
      std::array<std::bitset<32>, num_flag_levels>;
  using flags_container_type =
      std::array<flags_container_inner_type, sizeof...(Events)>;

  static constexpr size_t level_index(level lvl) {
    return std::to_underlying(lvl) - 1;
  }

  template<typename Event>
  static constexpr size_t event_index(Event event) {
    return static_cast<size_t>(std::to_underlying(event));
  }
public:
  using payload_type =
      std::variant<std::monostate, bool, uint32_t, int32_t, float>;
  struct logged_data_type {
    level lvl;
    std::variant<Events...> event;
    payload_type payload;
  };
private:
  flags_container_type flags_{};
  queue<logged_data_type, Capacity> data_;
public:
  template<typename Event>
    requires one_of<Event, Events...>
  constexpr void log(level lvl, Event event, payload_type payload = {}) {
    if (lvl != level::info) {
      flags_[type_index_v<Event, Events...>][level_index(lvl)].set(
          event_index(event)
      );
    }
    if (!data_.full()) {
      data_.push({lvl, event, payload});
    }
  }

  template<typename Event>
    requires one_of<Event, Events...>
  constexpr void info(Event event, payload_type payload = {}) {
    log(level::info, event, payload);
  }

  template<typename Event>
    requires one_of<Event, Events...>
  constexpr void notice(Event event, payload_type payload = {}) {
    log(level::notice, event, payload);
  }

  template<typename Event>
    requires one_of<Event, Events...>
  constexpr void warning(Event event, payload_type payload = {}) {
    log(level::warning, event, payload);
  }

  template<typename Event>
    requires one_of<Event, Events...>
  constexpr void error(Event event, payload_type payload = {}) {
    log(level::error, event, payload);
  }

  template<typename Event>
    requires one_of<Event, Events...>
  constexpr void critical(Event event, payload_type payload = {}) {
    log(level::critical, event, payload);
  }

  template<typename Event>
    requires one_of<Event, Events...>
  constexpr std::optional<level> test(Event event) {
    static constexpr std::array<level, 4> levels =
        {level::critical, level::error, level::warning, level::notice};
    for (auto lvl : levels) {
      if (flags_[type_index_v<Event, Events...>][level_index(lvl)]
               [event_index(event)]) {
        return lvl;
      }
    }
    return std::nullopt;
  }
};

} // namespace log
} // namespace emb
