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
namespace v2 {

namespace detail {

template<class T, class Ctx>
concept fsm_policy =
    requires {
      { T::entry_action } -> std::convertible_to<bool>;
      { T::exit_action } -> std::convertible_to<bool>;
    } && requires { typename T::template event_context_type<Ctx>; } &&
    std::is_lvalue_reference_v<typename T::template event_context_type<Ctx>> &&
    std::is_same_v<
        std::remove_cvref_t<typename T::template event_context_type<Ctx>>,
        std::remove_cv_t<Ctx>>;

template<class T, class Context, class State>
concept entry_action = requires(Context& cxt, State const& s) {
  { T::on_entry(cxt, s) } -> std::same_as<void>;
};

template<class T, class Context, class State>
concept exit_action = requires(Context& cxt, State const& s) {
  { T::on_exit(cxt, s) } -> std::same_as<void>;
};

template<
    typename T,
    typename Context,
    typename Policy,
    typename State,
    typename Event,
    typename TransitionResult>
concept event_handler =
    detail::fsm_policy<Policy, Context> &&
    requires(
        typename Policy::template event_context_type<Context> ctx,
        State const& s,
        Event&& event
    ) {
      {
        T::on_event(ctx, s, std::forward<Event>(event))
      } -> std::convertible_to<TransitionResult>;
    };

template<typename T, typename Context, typename Policy, typename... States>
concept fsm_transition_table = requires {
  requires detail::fsm_policy<Policy, Context>;
  requires(
      Policy::entry_action ? (detail::entry_action<T, Context, States> && ...) :
                             !(detail::entry_action<T, Context, States> || ...)
  );
  requires(
      Policy::exit_action ? (detail::exit_action<T, Context, States> && ...) :
                            !(detail::exit_action<T, Context, States> || ...)
  );
};

template<typename State>
concept state_with_id = requires {
  State::id;
  requires std::integral<std::remove_cv_t<decltype(State::id)>> ||
               std::is_enum_v<std::remove_cv_t<decltype(State::id)>>;
  requires requires {
    std::integral_constant<decltype(State::id), State::id>{};
  };
};

template<typename... States>
concept states_with_same_id_type =
    (sizeof...(States) > 0) &&
    std::conjunction_v<std::is_same<
        std::remove_cv_t<decltype(States::id)>,
        std::remove_cv_t<
            decltype(std::tuple_element_t<0, std::tuple<States...>>::id)>>...>;

template<typename... States>
concept states_with_unique_id = requires {
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

template<typename... States>
concept fsm_states = requires {
  requires sizeof...(States) > 0;
  requires(detail::state_with_id<States> && ...);
  requires detail::states_with_same_id_type<States...>;
  requires detail::states_with_unique_id<States...>;
};

template<typename State, typename... States>
concept one_of_fsm_states = std::disjunction_v<std::is_same<State, States>...>;

} // namespace detail

struct moore_policy {
  static constexpr bool entry_action = true;
  static constexpr bool exit_action = false;
  template<class Ctx>
  using event_context_type = Ctx const&;
};

struct mealy_policy {
  static constexpr bool entry_action = false;
  static constexpr bool exit_action = false;
  template<class Ctx>
  using event_context_type = Ctx&;
};

struct mixed_policy {
  static constexpr bool entry_action = true;
  static constexpr bool exit_action = true;
  template<class Ctx>
  using event_context_type = Ctx&;
};

template<
    typename Derived,
    typename Policy,
    typename Transitions,
    typename... States>
  requires detail::fsm_policy<Policy, Derived> &&
           detail::fsm_states<States...> &&
           detail::fsm_transition_table<Transitions, Derived, Policy, States...>
class finite_state_machine {
public:
  using fsm_type =
      finite_state_machine<Derived, Policy, Transitions, States...>;
  using context_type = Derived;
  using event_context_type =
      typename Policy::template event_context_type<context_type>;
  using transition_table = Transitions;
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
    requires detail::one_of_fsm_states<State, States...>
  constexpr finite_state_machine(State&& initial_state)
      : state_(std::forward<State>(initial_state)) {}

  template<typename Event>
    requires(
        detail::event_handler<
            transition_table,
            context_type,
            Policy,
            States,
            Event,
            next_state_type> &&
        ...
    )
  constexpr void dispatch(Event&& event) {
    auto const visitor = [&](auto const& s) -> next_state_type {
      event_context_type cxt = get_context();
      return transition_table::on_event(cxt, s, std::forward<Event>(event));
    };

    auto next_state = std::visit(visitor, state_);

    if (next_state) {
      exit_state();
      state_ = std::move(*next_state);
      enter_state();
    }
  }

  template<typename State, typename... Args>
    requires detail::one_of_fsm_states<State, States...>
  constexpr void force_transition(Args&&... args) {
    exit_state();
    state_ = state_type(std::in_place_type<State>, std::forward<Args>(args)...);
    enter_state();
  }
public:
  template<typename State, typename... Args>
    requires detail::one_of_fsm_states<State, States...>
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
    requires detail::one_of_fsm_states<State, States...>
  constexpr bool is_in_state() const {
    return std::holds_alternative<State>(state_);
  }

  [[nodiscard]] constexpr auto state_id() const {
    return std::visit(
        [](auto const& s) {
          using S = std::decay_t<decltype(s)>;
          return S::id;
        },
        state_
    );
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
    if constexpr (Policy::entry_action) {
      context_type& cxt = get_context();
      std::visit(
          [&](auto const& s) { transition_table::on_entry(cxt, s); },
          state_
      );
    }
  }

  constexpr void exit_state() {
    if constexpr (Policy::exit_action) {
      context_type& cxt = get_context();
      std::visit(
          [&](auto const& s) { transition_table::on_exit(cxt, s); },
          state_
      );
    }
  }
};

} // namespace v2
} // namespace fsm
} // namespace emb

#endif
