#pragma once

#include <emb/meta.hpp>

#include <concepts>
#include <type_traits>
#include <utility>
#include <variant>

namespace emb::fsm::command {

// A command goes from a control either to the state machine, as an event, or to
// whoever reads it, as data. Which control drives it is declared by the
// machine's current state, not assigned from outside: there is no owner to
// store, save or restore, and a state that names no control does not compile.
//
//   struct stopping {
//     using start_control = vcu_control;    // an event: start or stop
//     using run_control = stopping_control; // data: what the model runs on
//     ...
//   };
//
//   emb::fsm::command::deliver_all<channels>(drive);
//   auto const run = emb::fsm::command::read<run_channel>(drive);

namespace detail {

template<typename Channel>
concept declares_command = requires { typename Channel::command; };

} // namespace detail

// A channel ties a command to the name a state gives the control that drives
// it, and is the key the controls are overloaded on:
//
//   using power_command = std::variant<power_down, power_up>;
//
//   struct power_channel {
//     using command = power_command;
//     template<typename S>
//     using control = typename S::power_control;
//   };
//
//   static power_command value(power_channel, motor_drive const&); // a control
//
// The command is an event, or a std::variant of events of which the control
// picks one, and delivery dispatches it to the machine. A command that is data
// instead goes through read() and is never delivered.
//
// An alias template cannot be put into a typelist, so the channel is a struct
// that carries one. It is the key rather than its command type: a control may
// drive several channels, all through value(), and a function cannot be
// overloaded by its return type alone; and a control serves a channel only by
// naming it, never because a command type happens to match. The machine is
// keyed by the event instead: two channels that carry one event reach two
// controls and one handler, since the event is what the machine reacts to.
//
// Passed by value, so a channel is empty. Channels do not derive from one
// another: a derived one would convert to its base.
template<typename Channel>
concept some_channel = detail::declares_command<Channel>
                    && std::is_empty_v<Channel>
                    && std::default_initializable<Channel>;

namespace detail {

template<typename Control, typename Channel, typename Context>
concept projects = requires(Context const& ctx) {
  { Control::value(Channel{}, ctx) } -> std::same_as<typename Channel::command>;
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
template<typename Control, typename Channel, typename Context>
concept control_of = some_channel<Channel>
                  && std::is_empty_v<Control>
                  && detail::projects<Control, Channel, Context>;

namespace detail {

template<typename Command>
inline constexpr bool is_variant = false;

template<typename... Events>
inline constexpr bool is_variant<std::variant<Events...>> = true;

// Every state handles the event, or a common handler does for the states that
// do not: dispatch() in emb::fsm::v3 is constrained on exactly that.
template<typename Context, typename Event>
concept dispatches =
    requires(Context& ctx, Event event) { ctx.dispatch(std::move(event)); };

template<typename Context, typename Command>
inline constexpr bool dispatches_command = dispatches<Context, Command>;

template<typename Context, typename... Events>
inline constexpr bool dispatches_command<Context, std::variant<Events...>> =
    (dispatches<Context, Events> && ...);

} // namespace detail

// The context dispatches every event the channel's command can hold.
template<typename Context, typename Channel>
concept dispatcher_of =
    some_channel<Channel>
    && detail::dispatches_command<Context, typename Channel::command>;

// The state names a control for the channel.
template<typename Channel, typename State, typename Context>
concept channel_for =
    some_channel<Channel>
    && control_of<typename Channel::template control<State>, Channel, Context>;

namespace detail {

template<typename Machine>
concept publishes_states = requires { typename Machine::state_list; }
                        && some_typelist<typename Machine::state_list>;

template<typename Channel, typename StateList, typename Context>
inline constexpr bool controls_every_state = false;

template<typename Channel, typename... States, typename Context>
inline constexpr bool
    controls_every_state<Channel, typelist<States...>, Context> =
        (channel_for<Channel, States, Context> && ...);

} // namespace detail

// Every state of the context names a control for the channel. The context is
// the state machine and what the controls read, both at once: on a drive it is
// one object.
template<typename Channel, typename Context>
concept readable = some_channel<Channel>
                && detail::publishes_states<Context>
                && detail::controls_every_state<Channel,
                                                typename Context::state_list,
                                                Context>;

// The channel is readable, and the context dispatches every event its command
// can hold.
template<typename Channel, typename Context>
concept deliverable =
    readable<Channel, Context> && dispatcher_of<Context, Channel>;

// -------------------------------------------------------------------- reading

// Returns the command the control named by the current state gives `Channel`.
//
// `deliver()` is this and a dispatch. A channel that carries data rather than
// an event is read here and never delivered, and the caller decides what
// becomes of the command.
template<typename Channel, typename Context>
  requires readable<Channel, Context>
constexpr typename Channel::command read(Context const& ctx)
{
  using Command = typename Channel::command;
  return ctx.visit([&ctx](auto const& state) -> Command {
    using State = std::remove_cvref_t<decltype(state)>;
    return Channel::template control<State>::value(Channel{}, ctx);
  });
}

// ------------------------------------------------------------------- delivery

// Reads which control the current state names for the channel, and dispatches
// that control's command to the machine: the event, or the one a std::variant
// holds.
template<typename Channel, typename Context>
  requires deliverable<Channel, Context>
constexpr void deliver(Context& ctx)
{
  using Command = typename Channel::command;
  Command cmd = command::read<Channel>(ctx);
  // dispatched only after read() returns: inside its visit, a transition
  // would replace the state the visitor still holds a reference to
  if constexpr (detail::is_variant<Command>) {
    std::visit([&ctx](auto& event) { ctx.dispatch(std::move(event)); }, cmd);
  }
  else {
    ctx.dispatch(std::move(cmd));
  }
}

namespace detail {

template<typename... Channels, typename Context>
constexpr void deliver_each(typelist<Channels...>, Context& ctx)
{
  (deliver<Channels>(ctx), ...);
}

} // namespace detail

// Every channel in one call, in list order: a fourth channel is a fourth entry
// in the list, not a fourth line somebody has to remember in an interrupt
// handler. The order is observable -- delivering a command may move the state
// machine, and the channels after it read the state it moved to. A channel is
// listed once; listed twice, it would be dispatched twice per pass.
//
// Delivered on every call, not on change: a command is a request the control
// holds, and a state that could not act on it yet relies on seeing it again. A
// channel's events must therefore bear repeating; a one-shot event is
// dispatched by whoever raises it, not carried by a channel.
template<typename ChannelList, typename Context>
  requires some_typelist<ChannelList>
        && (ChannelList::size > 0)
        && typelist_unique<ChannelList>
constexpr void deliver_all(Context& ctx)
{
  detail::deliver_each(ChannelList{}, ctx);
}

} // namespace emb::fsm::command
