#pragma once

#if __cplusplus >= 202300

#include <array>
#include <concepts>
#include <optional>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>

namespace emb {
namespace fsm {
namespace v3 {

namespace detail {

template<class T, class Ctx>
concept FsmPolicy =
    requires {
      { T::has_entry_action } -> std::convertible_to<bool>;
      { T::has_exit_action } -> std::convertible_to<bool>;
    } && requires { typename T::template event_context_type<Ctx>; } &&
    std::is_lvalue_reference_v<typename T::template event_context_type<Ctx>> &&
    std::is_same_v<
        std::remove_cvref_t<typename T::template event_context_type<Ctx>>,
        std::remove_cv_t<Ctx>>;

template<typename State, typename Context>
concept EntryAction = requires(Context& cxt) {
  { State::on_entry(cxt) } -> std::same_as<void>;
};

template<typename State, typename Context>
concept ExitAction = requires(Context& cxt) {
  { State::on_exit(cxt) } -> std::same_as<void>;
};

template<typename State>
concept StateWithId = requires {
  State::id;
  requires std::integral<std::remove_cv_t<decltype(State::id)>> ||
               std::is_enum_v<std::remove_cv_t<decltype(State::id)>>;
  requires requires {
    std::integral_constant<decltype(State::id), State::id>{};
  };
};

template<typename... States>
concept StatesWithSameIdType =
    (sizeof...(States) > 0) &&
    (std::conjunction_v<std::is_same<
         std::remove_cv_t<decltype(States::id)>,
         std::tuple_element_t<
             0,
             std::tuple<std::remove_cv_t<decltype(States::id)>...>>>...>);

template<typename... States>
concept StatesWithUniqueIds = requires {
  requires[]() constexpr->bool {
    if constexpr (sizeof...(States) <= 1) {
      return true;
    } else {
      constexpr std::array ids = {States::id...};
      for (std::size_t i = 0; i < ids.size(); ++i) {
        for (std::size_t j = i + 1; j < ids.size(); ++j) {
          if (ids[i] == ids[j]) {
            return false;
          }
        }
      }
      return true;
    }
  }
  ();
};

template<typename Context, typename Policy, typename... States>
concept FsmStates = requires {
  requires sizeof...(States) > 0;
  requires(detail::StateWithId<States> && ...);
  requires detail::StatesWithSameIdType<States...>;
  requires detail::StatesWithUniqueIds<States...>;
  requires detail::FsmPolicy<Policy, Context>;
  requires(
      Policy::has_entry_action ?
          (detail::EntryAction<States, Context> && ...) :
          !(detail::EntryAction<States, Context> || ...));
  requires(
      Policy::has_exit_action ? (detail::ExitAction<States, Context> && ...) :
                                !(detail::ExitAction<States, Context> || ...));
};

template<typename State, typename... States>
concept OneOfFsmStates = std::disjunction_v<std::is_same<State, States>...>;

template<
    typename State,
    typename Context,
    typename Policy,
    typename Event,
    typename Result>
concept StateEventHandler =
    detail::FsmPolicy<Policy, Context> &&
    requires(
        typename Policy::template event_context_type<Context> ctx,
        Event&& event) {
      {
        State::on_event(ctx, std::forward<Event>(event))
      } -> std::convertible_to<Result>;
    };

template<typename Context, typename Policy, typename Event, typename Result>
concept CommonEventHandler =
    detail::FsmPolicy<Policy, Context> &&
    requires(
        typename Policy::template event_context_type<Context> ctx,
        Event&& event) {
      {
        on_event(ctx, std::forward<Event>(event))
      } -> std::convertible_to<Result>;
    };

template<
    typename State,
    typename Context,
    typename Policy,
    typename Event,
    typename Result>
concept EventHandler =
    (static_cast<int>(
         StateEventHandler<State, Context, Policy, Event, Result>) +
     static_cast<int>(
         CommonEventHandler<Context, Policy, Event, Result>)) == 1;

} // namespace detail

struct moore_policy {
  static constexpr bool has_entry_action = true;
  static constexpr bool has_exit_action = false;
  template<class Ctx>
  using event_context_type = Ctx const&;
};

struct mealy_policy {
  static constexpr bool has_entry_action = false;
  static constexpr bool has_exit_action = false;
  template<class Ctx>
  using event_context_type = Ctx&;
};

struct mixed_policy {
  static constexpr bool has_entry_action = true;
  static constexpr bool has_exit_action = true;
  template<class Ctx>
  using event_context_type = Ctx&;
};

template<typename Derived, typename Policy, typename... States>
  requires detail::FsmPolicy<Policy, Derived> &&
           detail::FsmStates<Derived, Policy, States...>
class finite_state_machine {
public:
  using fsm_type = finite_state_machine<Derived, Policy, States...>;
  using context_type = Derived;
  using event_context_type =
      typename Policy::template event_context_type<context_type>;
  using state_type = std::variant<States...>;
  using next_state_type = std::optional<state_type>;
private:
  state_type state_;
public:
  ~finite_state_machine() = default;
  finite_state_machine(finite_state_machine const&) = delete;
  finite_state_machine& operator=(finite_state_machine const&) = delete;
  finite_state_machine(finite_state_machine&&) = default;
  finite_state_machine& operator=(finite_state_machine&&) = default;

  template<typename State>
    requires detail::OneOfFsmStates<State, States...>
  constexpr finite_state_machine(State&& initial_state)
      : state_(std::forward<State>(initial_state)) {}

  // clang-format off

  template<typename Event>
    requires(detail::EventHandler<States, context_type, Policy, Event, next_state_type> && ...)
  constexpr void dispatch(Event&& event) {
    auto const visitor = [&](auto const& s) -> next_state_type {
      using S = std::decay_t<decltype(s)>;
      event_context_type cxt = get_context();
      if constexpr (detail::StateEventHandler<S, context_type, Policy, Event, next_state_type>) {
        return S::on_event(cxt, std::forward<Event>(event));
      } else if constexpr (detail::CommonEventHandler<context_type, Policy, Event, next_state_type>) {
        return on_event(cxt, std::forward<Event>(event));
      } else {
        return no_transition();
      }
    };

    auto next_state = std::visit(visitor, state_);

    if (next_state) {
      exit_state();
      state_ = std::move(*next_state);
      enter_state();
    }
  }

  // clang-format on

  template<typename State, typename... Args>
    requires detail::OneOfFsmStates<State, States...>
  constexpr void force_transition(Args&&... args) {
    exit_state();
    state_ = state_type(std::in_place_type<State>, std::forward<Args>(args)...);
    enter_state();
  }
public:
  template<typename State, typename... Args>
    requires detail::OneOfFsmStates<State, States...>
  [[nodiscard]] static constexpr next_state_type transition_to(Args&&... args) {
    return state_type(std::in_place_type<State>, std::forward<Args>(args)...);
  }

  [[nodiscard]] static constexpr next_state_type no_transition() {
    return std::nullopt;
  }
public:
  [[nodiscard]] constexpr state_type const& state() const {
    return state_;
  }

  template<typename Visitor>
  constexpr decltype(auto) visit(Visitor&& visitor) const {
    return std::visit(std::forward<Visitor>(visitor), state_);
  }

  template<typename State>
    requires detail::OneOfFsmStates<State, States...>
  constexpr bool is_in_state() const {
    return std::holds_alternative<State>(state_);
  }

  [[nodiscard]] constexpr auto state_id() const {
    return std::visit(
        [](auto const& s) {
          using S = std::decay_t<decltype(s)>;
          return S::id;
        },
        state_);
  }
protected:
  constexpr void start_fsm() {
    enter_state();
  }

  constexpr void stop_fsm() {
    exit_state();
  }
private:
  [[nodiscard]] constexpr context_type& get_context() {
    return static_cast<context_type&>(*this);
  }

  [[nodiscard]] constexpr context_type const& get_context() const {
    return static_cast<context_type const&>(*this);
  }

  constexpr void enter_state() {
    if constexpr (Policy::has_entry_action) {
      context_type& cxt = get_context();
      std::visit(
          [&](auto const& s) {
            using S = std::decay_t<decltype(s)>;
            S::on_entry(cxt);
          },
          state_);
    }
  }

  constexpr void exit_state() {
    if constexpr (Policy::has_exit_action) {
      context_type& cxt = get_context();
      std::visit(
          [&](auto const& s) {
            using S = std::decay_t<decltype(s)>;
            S::on_exit(cxt);
          },
          state_);
    }
  }
};

} // namespace v3
} // namespace fsm
} // namespace emb

#endif
