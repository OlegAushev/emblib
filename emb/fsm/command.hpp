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

namespace detail {

template<typename Channel>
concept declares_command = requires { typename Channel::command; };

} // namespace detail

// A channel ties a command to the name a state gives the control that drives
// it, and is the key both ends are overloaded on:
//
//   struct run_channel {
//     using command = run_command;
//     template<typename S>
//     using control = typename S::run_control;
//   };
//
//   static run_command value(run_channel, motor_drive const&); // a control
//   void accept(run_channel, run_command const&);              // the sink
//
// An alias template cannot be put into a typelist, so the channel is a struct
// that carries one. It is the key rather than its command type: a control may
// drive several channels, all through value(), and a function cannot be
// overloaded by its return type alone; two channels may carry one command type
// and still reach different controls and different accept() overloads; and a
// control or a sink serves a channel only by naming it, never because a command
// type happens to match.
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

template<typename Sink, typename Channel>
concept sink_of = some_channel<Channel>
               && requires(Sink& sink, typename Channel::command const& cmd) {
                    { sink.accept(Channel{}, cmd) } -> std::same_as<void>;
                  };

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

// Every state of the context names a control for the channel, and the context
// accepts the channel. The context is the state machine, what the controls read
// and the sink, all at once: on a drive it is one object.
//
// This is the testable form of what deliver() insists on with sentences.
template<typename Channel, typename Context>
concept deliverable = some_channel<Channel>
                   && detail::publishes_states<Context>
                   && sink_of<Context, Channel>
                   && detail::controls_every_state<Channel,
                                                   typename Context::state_list,
                                                   Context>;

// ---------------------------------------------------------------- diagnostics

namespace detail {

template<typename...>
inline constexpr bool always_false = false;

template<typename Channel, typename State>
concept names_control = requires { typename Channel::template control<State>; };

template<typename Channel, typename State, typename Context, bool Proceed>
struct diagnose_control {
  static constexpr bool ok = true; // an earlier requirement failed; stop here
};

template<typename Channel, typename State, typename Context>
struct diagnose_control<Channel, State, Context, true> {
  using control_type = typename Channel::template control<State>;

  static_assert(
      std::is_empty_v<control_type>,
      "emb::fsm::command: a control must be empty: it projects the context "
      "onto a command and keeps nothing, and what it reads belongs to the "
      "context");
  static_assert(
      projects<control_type, Channel, Context>,
      "emb::fsm::command: a control must define `static C value(Channel, "
      "context const&)` for the channel, returning exactly its command type");
  static constexpr bool ok = true;
};

template<typename Channel, typename State, typename Context>
struct diagnose_state {
  static_assert(
      names_control<Channel, State>,
      "emb::fsm::command: every state must name the control that drives this "
      "channel; a state in which nobody drives it names an idle control");
  static constexpr bool ok =
      diagnose_control<Channel,
                       State,
                       Context,
                       names_control<Channel, State>>::ok;
};

template<typename Channel, typename StateList, typename Context>
struct diagnose_states;

template<typename Channel, typename... States, typename Context>
struct diagnose_states<Channel, typelist<States...>, Context> {
  static constexpr bool ok =
      (diagnose_state<Channel, States, Context>::ok && ...);
};

template<typename Channel, typename Context, bool Proceed>
struct diagnose_delivery_details {
  static constexpr bool ok = true; // an earlier requirement failed; stop here
};

template<typename Channel, typename Context>
struct diagnose_delivery_details<Channel, Context, true> {
  static_assert(
      sink_of<Context, Channel>,
      "emb::fsm::command: the context must accept every channel delivered to "
      "it as `void accept(Channel, C const&)`");
  static constexpr bool ok =
      diagnose_states<Channel, typename Context::state_list, Context>::ok;
};

template<typename Channel, typename Context>
struct diagnose_delivery {
  static_assert(declares_command<Channel>,
                "emb::fsm::command: a channel must declare the command it "
                "carries as `using command = ...`");
  static_assert(
      std::is_empty_v<Channel> && std::default_initializable<Channel>,
      "emb::fsm::command: a channel is the key value() and accept() are "
      "overloaded on and is passed by value, so it must be empty and "
      "default-constructible");
  static_assert(
      publishes_states<Context>,
      "emb::fsm::command: the context must be a state machine that publishes "
      "its states as `state_list`, an emb::typelist; "
      "emb::fsm::v3::finite_state_machine does");
  static constexpr bool ok = diagnose_delivery_details < Channel, Context,
                        some_channel<Channel>&&publishes_states
                            < Context
                            >> ::ok;
};

template<typename ChannelList, typename Context>
struct diagnose_channels {
  static_assert(always_false<ChannelList>,
                "emb::fsm::command::deliver_all: the channel list must be "
                "emb::typelist<Channels...>");
  static constexpr bool ok = true;
};

template<typename... Channels, typename Context>
struct diagnose_channels<typelist<Channels...>, Context> {
  static_assert(
      sizeof...(Channels) > 0,
      "emb::fsm::command::deliver_all: at least one channel is required");
  static_assert(
      typelist_unique<typelist<Channels...>>,
      "emb::fsm::command::deliver_all: a channel is listed twice, and its "
      "command would reach the sink twice per pass");
  static_assert((diagnose_delivery<Channels, Context>::ok && ...));
  static constexpr bool ok = true;
};

} // namespace detail

// ------------------------------------------------------------------- delivery

// Reads which control the current state names for the channel, and hands that
// control's value to the sink.
template<typename Channel, typename Context>
constexpr void deliver(Context& ctx)
{
  static_assert(detail::diagnose_delivery<Channel, Context>::ok);
  if constexpr (deliverable<Channel, Context>) {
    using Command = typename Channel::command;
    // read inside the visit and accept outside it: accepting a command may
    // move the state machine, and the state visit() looks at would be gone
    Command const cmd = ctx.visit([&ctx](auto const& state) -> Command {
      using State = std::remove_cvref_t<decltype(state)>;
      return Channel::template control<State>::value(Channel{}, ctx);
    });
    ctx.accept(Channel{}, cmd);
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
// machine, and the channels after it read the state it moved to.
//
// Delivered on every call, not on change: a sink that turns a held level into
// an event relies on seeing it again.
template<typename ChannelList, typename Context>
constexpr void deliver_all(Context& ctx)
{
  static_assert(detail::diagnose_channels<ChannelList, Context>::ok);
  if constexpr (some_typelist<ChannelList>) {
    detail::deliver_each(ChannelList{}, ctx);
  }
}

} // namespace emb::fsm::command
