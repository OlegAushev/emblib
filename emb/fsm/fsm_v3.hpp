#pragma once

#if __cplusplus >= 202300

#include <emb/meta.hpp>

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
concept fsm_policy =
    requires {
      { T::entry_action } -> std::convertible_to<bool>;
      { T::exit_action } -> std::convertible_to<bool>;
    } &&
    requires { typename T::template event_context_type<Ctx>; } &&
    std::is_lvalue_reference_v<typename T::template event_context_type<Ctx>> &&
    std::is_same_v<
        std::remove_cvref_t<typename T::template event_context_type<Ctx>>,
        std::remove_cv_t<Ctx>>;

template<typename State, typename Context>
concept entry_action = requires(Context& ctx) {
  { State::on_entry(ctx) } -> std::same_as<void>;
};

template<typename State, typename Context>
concept exit_action = requires(Context& ctx) {
  { State::on_exit(ctx) } -> std::same_as<void>;
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
consteval bool are_id_unique() {
  if constexpr (sizeof...(States) <= 1) {
    return true;
  } else {
    constexpr std::array ids = {States::id...};
    for (auto i = 0uz; i < ids.size(); ++i) {
      for (auto j = i + 1; j < ids.size(); ++j) {
        if (ids[i] == ids[j]) {
          return false;
        }
      }
    }
    return true;
  }
}

template<typename... States>
concept states_with_unique_id = are_id_unique<States...>();

template<
    typename State,
    typename Context,
    typename Policy,
    typename Event,
    typename Result>
concept state_event_handler =
    detail::fsm_policy<Policy, Context> &&
    requires(
        typename Policy::template event_context_type<Context> ctx,
        Event&& event
    ) {
      {
        State::on_event(ctx, std::forward<Event>(event))
      } -> std::convertible_to<Result>;
    };

template<typename Context, typename Policy, typename Event, typename Result>
concept common_event_handler =
    detail::fsm_policy<Policy, Context> &&
    requires(
        typename Policy::template event_context_type<Context> ctx,
        Event&& event
    ) {
      {
        on_event(ctx, std::forward<Event>(event))
      } -> std::convertible_to<Result>;
    };

template<
    typename Context,
    typename Policy,
    typename Event,
    typename Result,
    typename... States>
concept event_handler =
    ((state_event_handler<States, Context, Policy, Event, Result> ||
      common_event_handler<Context, Policy, Event, Result>) &&
     ...);

template<typename...>
inline constexpr bool always_false = false;

template<typename State>
struct diagnose_state {
  static_assert(
      state_with_id<State>,
      "finite_state_machine: every state must declare a static `id` member "
      "(an integral or enum constant)"
  );
  static constexpr bool ok = true;
};

template<typename Context, typename Policy, bool Proceed, typename... States>
struct diagnose_fsm_details {
  static constexpr bool ok = true; // an earlier requirement failed; stop here
};

template<typename Context, typename Policy, typename... States>
struct diagnose_fsm_details<Context, Policy, true, States...> {
  static_assert(
      states_with_same_id_type<States...>,
      "finite_state_machine: all State::id must share the same type"
  );
  static_assert(
      states_with_unique_id<States...>,
      "finite_state_machine: all State::id values must be unique"
  );
  static_assert(
      !Policy::entry_action || (entry_action<States, Context> && ...),
      "finite_state_machine: this policy requires every state to define "
      "on_entry(context&)"
  );
  static_assert(
      Policy::entry_action || !(entry_action<States, Context> || ...),
      "finite_state_machine: this policy forbids states from defining "
      "on_entry(context&)"
  );
  static_assert(
      !Policy::exit_action || (exit_action<States, Context> && ...),
      "finite_state_machine: this policy requires every state to define "
      "on_exit(context&)"
  );
  static_assert(
      Policy::exit_action || !(exit_action<States, Context> || ...),
      "finite_state_machine: this policy forbids states from defining "
      "on_exit(context&)"
  );
  static constexpr bool ok = true;
};

template<typename Derived, typename Policy, typename StateList>
struct diagnose_fsm {
  static_assert(
      always_false<StateList>,
      "finite_state_machine: the state-list argument must be "
      "emb::typelist<States...>"
  );
  static constexpr bool ok = true;
};

template<typename Derived, typename Policy, typename... States>
struct diagnose_fsm<Derived, Policy, typelist<States...>> {
  static_assert(
      fsm_policy<Policy, Derived>,
      "finite_state_machine: Policy must model fsm_policy (entry_action, "
      "exit_action, event_context_type<context>)"
  );
  static_assert(
      sizeof...(States) > 0,
      "finite_state_machine: at least one state is required"
  );
  static_assert((diagnose_state<States>::ok && ...));
  static constexpr bool ok = diagnose_fsm_details<
      Derived,
      Policy,
      fsm_policy<Policy, Derived> && (state_with_id<States> && ...),
      States...>::ok;
};

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

template<typename Derived, typename Policy, typename StateList>
class finite_state_machine {
  static_assert(detail::diagnose_fsm<Derived, Policy, StateList>::ok);
};

template<typename Derived, typename Policy, typename... States>
class finite_state_machine<Derived, Policy, typelist<States...>> {
  static_assert(detail::diagnose_fsm<Derived, Policy, typelist<States...>>::ok);
public:
  using state_list = typelist<States...>;
  using fsm_type = finite_state_machine<Derived, Policy, state_list>;
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
    requires typelist_contains<state_list, State>
  constexpr finite_state_machine(State&& initial_state)
      : state_(std::forward<State>(initial_state)) {}

  template<typename Event>
    requires(detail::event_handler<
             context_type,
             Policy,
             Event,
             next_state_type,
             States...>)
  constexpr void dispatch(Event&& event) {
    auto const visitor = [&](auto const& s) -> next_state_type {
      using S = std::remove_cvref_t<decltype(s)>;
      event_context_type ctx = get_context();
      if constexpr (detail::state_event_handler<
                        S,
                        context_type,
                        Policy,
                        Event,
                        next_state_type>) {
        return S::on_event(ctx, std::forward<Event>(event));
      } else if constexpr (detail::common_event_handler<
                               context_type,
                               Policy,
                               Event,
                               next_state_type>) {
        return on_event(ctx, std::forward<Event>(event));
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

  template<typename State, typename... Args>
    requires typelist_contains<state_list, State>
  constexpr void force_transition(Args&&... args) {
    exit_state();
    state_ = state_type(std::in_place_type<State>, std::forward<Args>(args)...);
    enter_state();
  }
public:
  template<typename State, typename... Args>
    requires typelist_contains<state_list, State>
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
    requires typelist_contains<state_list, State>
  constexpr bool is_in_state() const {
    return std::holds_alternative<State>(state_);
  }

  [[nodiscard]] constexpr auto state_id() const {
    return std::visit(
        [](auto const& s) {
          using S = std::remove_cvref_t<decltype(s)>;
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
      context_type& ctx = get_context();
      std::visit(
          [&](auto const& s) {
            using S = std::remove_cvref_t<decltype(s)>;
            S::on_entry(ctx);
          },
          state_
      );
    }
  }

  constexpr void exit_state() {
    if constexpr (Policy::exit_action) {
      context_type& ctx = get_context();
      std::visit(
          [&](auto const& s) {
            using S = std::remove_cvref_t<decltype(s)>;
            S::on_exit(ctx);
          },
          state_
      );
    }
  }
};

} // namespace v3
} // namespace fsm
} // namespace emb

#endif
