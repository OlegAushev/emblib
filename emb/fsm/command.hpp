#pragma once

#include <emb/meta.hpp>

#include <concepts>
#include <type_traits>

namespace emb::fsm::command {

// A command goes from a control to a sink. Which control drives it is declared
// by the sink's current state, not assigned from outside: there is no owner to
// store, save or restore, and a state that names no control does not compile.
//
//   struct stopping {
//     using run_control = stopping_control;
//     ...
//   };
//
//   emb::fsm::command::deliver_all<channels>(drive);

// The first parameter on both sides. Overloads are then chosen by the command
// and never by a conversion of its value: two commands with one underlying type
// stay apart, and an accept(int) cannot quietly take a bool.
template<typename C>
struct tag {};

namespace detail {

template<typename K, typename C, typename Ctx>
concept projects = requires(Ctx const& ctx) {
  { K::value(tag<C>{}, ctx) } -> std::same_as<C>;
};

} // namespace detail

// A control projects the context onto a command and keeps nothing. Emptiness is
// not an optimization: it is what removes lifetimes and save/restore as a class
// of mistake, since a state has nothing to acquire and so nothing to hand back.
// A control that needs an outside object reads it from the context.
//
// std::is_empty_v does not see static members, so "and no static state either"
// is a review rule that this concept cannot check.
//
// same_as rather than convertible_to: a control returning int where the command
// is long is a bug, not a conversion.
template<typename K, typename C, typename Ctx>
concept control_of = std::is_empty_v<K> && detail::projects<K, C, Ctx>;

template<typename K, typename C>
concept sink_of = requires(K& k, C const& cmd) {
  { k.accept(tag<C>{}, cmd) } -> std::same_as<void>;
};

// A channel ties a command to the name a state gives the control that drives
// it. An alias template cannot be put into a typelist, so the channel is a
// struct that carries one, and declares the command beside it:
//
//   struct run_channel {
//     using command_type = run_cmd;
//     template<typename S>
//     using control = typename S::run_control;
//   };
template<typename Ch, typename S, typename Ctx>
concept channel_for = requires { typename Ch::command_type; }
                   && control_of<typename Ch::template control<S>,
                                 typename Ch::command_type,
                                 Ctx>;

namespace detail {

template<typename M>
concept publishes_states = requires { typename M::state_list; }
                        && some_typelist<typename M::state_list>;

template<typename Ch, typename StateList, typename Ctx>
inline constexpr bool controls_every_state = false;

template<typename Ch, typename... States, typename Ctx>
inline constexpr bool controls_every_state<Ch, typelist<States...>, Ctx> =
    (channel_for<Ch, States, Ctx> && ...);

} // namespace detail

// Every state of the context names a control for the channel's command, and the
// context accepts that command. The context is the state machine, what the
// controls read and the sink, all at once: on a drive it is one object.
//
// This is the testable form of what deliver() insists on with sentences.
template<typename Ch, typename Ctx>
concept deliverable =
    requires { typename Ch::command_type; }
    && detail::publishes_states<Ctx>
    && sink_of<Ctx, typename Ch::command_type>
    && detail::controls_every_state<Ch, typename Ctx::state_list, Ctx>;

// ---------------------------------------------------------------- diagnostics

namespace detail {

template<typename...>
inline constexpr bool always_false = false;

template<typename Ch>
concept declares_command = requires { typename Ch::command_type; };

template<typename Ch, typename S>
concept names_control = requires { typename Ch::template control<S>; };

template<typename Ch, typename S, typename Ctx, bool Proceed>
struct diagnose_control {
  static constexpr bool ok = true; // an earlier requirement failed; stop here
};

template<typename Ch, typename S, typename Ctx>
struct diagnose_control<Ch, S, Ctx, true> {
  using control_type = typename Ch::template control<S>;
  using command_type = typename Ch::command_type;

  static_assert(
      std::is_empty_v<control_type>,
      "emb::fsm::command: a control must be empty: it projects the context "
      "onto a command and keeps nothing, and what it reads belongs to the "
      "context");
  static_assert(
      projects<control_type, command_type, Ctx>,
      "emb::fsm::command: a control must define `static C value(tag<C>, "
      "context const&)` returning exactly the command type of the channel");
  static constexpr bool ok = true;
};

template<typename Ch, typename S, typename Ctx>
struct diagnose_state {
  static_assert(
      names_control<Ch, S>,
      "emb::fsm::command: every state must name the control that drives this "
      "command; a state in which nobody drives it names an idle control");
  static constexpr bool ok =
      diagnose_control<Ch, S, Ctx, names_control<Ch, S>>::ok;
};

template<typename Ch, typename StateList, typename Ctx>
struct diagnose_states;

template<typename Ch, typename... States, typename Ctx>
struct diagnose_states<Ch, typelist<States...>, Ctx> {
  static constexpr bool ok = (diagnose_state<Ch, States, Ctx>::ok && ...);
};

template<typename Ch, typename Ctx, bool Proceed>
struct diagnose_delivery_details {
  static constexpr bool ok = true; // an earlier requirement failed; stop here
};

template<typename Ch, typename Ctx>
struct diagnose_delivery_details<Ch, Ctx, true> {
  static_assert(
      sink_of<Ctx, typename Ch::command_type>,
      "emb::fsm::command: the context must accept every command delivered to "
      "it as `void accept(emb::fsm::command::tag<C>, C const&)`");
  static constexpr bool ok =
      diagnose_states<Ch, typename Ctx::state_list, Ctx>::ok;
};

template<typename Ch, typename Ctx>
struct diagnose_delivery {
  static_assert(declares_command<Ch>,
                "emb::fsm::command: a channel must declare the command it "
                "carries as `using command_type = ...`");
  static_assert(
      publishes_states<Ctx>,
      "emb::fsm::command: the context must be a state machine that publishes "
      "its states as `state_list`, an emb::typelist; "
      "emb::fsm::v3::finite_state_machine does");
  static constexpr bool ok = diagnose_delivery_details < Ch, Ctx,
                        declares_command<Ch>&&publishes_states < Ctx >> ::ok;
};

template<bool Proceed, typename... Channels>
struct diagnose_commands {
  static constexpr bool ok = true; // an earlier requirement failed; stop here
};

template<typename... Channels>
struct diagnose_commands<true, Channels...> {
  static_assert(
      typelist_unique<typelist<typename Channels::command_type...>>,
      "emb::fsm::command::deliver_all: two channels carry the same command, "
      "which would reach the sink twice per pass");
  static constexpr bool ok = true;
};

template<typename ChannelList, typename Ctx>
struct diagnose_channels {
  static_assert(always_false<ChannelList>,
                "emb::fsm::command::deliver_all: the channel list must be "
                "emb::typelist<Channels...>");
  static constexpr bool ok = true;
};

template<typename... Channels, typename Ctx>
struct diagnose_channels<typelist<Channels...>, Ctx> {
  static_assert(
      sizeof...(Channels) > 0,
      "emb::fsm::command::deliver_all: at least one channel is required");
  static_assert((diagnose_delivery<Channels, Ctx>::ok && ...));
  static constexpr bool ok =
      diagnose_commands<(declares_command<Channels> && ...), Channels...>::ok;
};

} // namespace detail

// ------------------------------------------------------------------- delivery

// Reads which control the current state names for the command of the channel,
// and hands that control's value to the sink.
template<typename Ch, typename Ctx>
constexpr void deliver(Ctx& ctx)
{
  static_assert(detail::diagnose_delivery<Ch, Ctx>::ok);
  if constexpr (deliverable<Ch, Ctx>) {
    using C = typename Ch::command_type;
    // read inside the visit and accept outside it: accepting a command may
    // move the state machine, and the state visit() looks at would be gone
    C const cmd = ctx.visit([&ctx](auto const& state) -> C {
      using S = std::remove_cvref_t<decltype(state)>;
      return Ch::template control<S>::value(tag<C>{}, ctx);
    });
    ctx.accept(tag<C>{}, cmd);
  }
}

namespace detail {

template<typename... Channels, typename Ctx>
constexpr void deliver_each(typelist<Channels...>, Ctx& ctx)
{
  (deliver<Channels>(ctx), ...);
}

} // namespace detail

// Every channel in one call, in list order: a fourth command is a fourth entry
// in the list, not a fourth line somebody has to remember in an interrupt
// handler. The order is observable -- delivering a command may move the state
// machine, and the channels after it read the state it moved to.
//
// Delivered on every call, not on change: a sink that turns a held level into
// an event relies on seeing it again.
template<typename ChannelList, typename Ctx>
constexpr void deliver_all(Ctx& ctx)
{
  static_assert(detail::diagnose_channels<ChannelList, Ctx>::ok);
  if constexpr (some_typelist<ChannelList>) {
    detail::deliver_each(ChannelList{}, ctx);
  }
}

} // namespace emb::fsm::command
