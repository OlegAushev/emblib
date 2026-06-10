#include <emb/fsm/fsm_v2.hpp>

#include <cassert>

namespace {

namespace moore_fsm {

class Switch;

struct OpenEvent {};

struct CloseEvent {};

struct UpdateEvent {};

enum class SwitchStateTag { open, closed };

struct OpenState {
  static constexpr SwitchStateTag tag{SwitchStateTag::open};
};

struct ClosedState {
  static constexpr SwitchStateTag tag{SwitchStateTag::closed};
};

struct Transitions {
  static constexpr void on_entry(Switch&, OpenState const&);
  static constexpr auto on_event(
      Switch const&,
      OpenState const&,
      OpenEvent const&
  );
  static constexpr auto on_event(
      Switch const&,
      OpenState const&,
      CloseEvent const&
  );
  static constexpr void on_entry(Switch&, ClosedState const&);
  static constexpr auto on_event(
      Switch const&,
      ClosedState const&,
      OpenEvent const&
  );
  static constexpr auto on_event(
      Switch const&,
      ClosedState const&,
      CloseEvent const&
  );
};

class Switch : public emb::fsm::v2::finite_state_machine<
                   Switch,
                   emb::fsm::v2::moore_policy,
                   Transitions,
                   OpenState,
                   ClosedState> {
public:
  constexpr Switch() : fsm_type(ClosedState{}) {
    start_fsm();
  }

  constexpr ~Switch() {
    stop_fsm();
  }

  int open_entries = 0;
  int closed_entries = 0;
};

constexpr void Transitions::on_entry(Switch& s, OpenState const&) {
  ++s.open_entries;
}

constexpr auto Transitions::on_event(
    Switch const& s,
    OpenState const&,
    OpenEvent const&
) {
  return std::nullopt;
}

constexpr auto Transitions::on_event(
    Switch const& s,
    OpenState const&,
    CloseEvent const&
) {
  return ClosedState{};
}

constexpr void Transitions::on_entry(Switch& s, ClosedState const&) {
  ++s.closed_entries;
}

constexpr auto Transitions::on_event(
    Switch const& s,
    ClosedState const&,
    OpenEvent const&
) {
  return OpenState{};
}

constexpr auto Transitions::on_event(
    Switch const& s,
    ClosedState const&,
    CloseEvent const&
) {
  return std::nullopt;
}

struct SwitchVisitor {
  constexpr SwitchStateTag operator()(OpenState const&) {
    return SwitchStateTag::open;
  }

  constexpr SwitchStateTag operator()(ClosedState const&) {
    return SwitchStateTag::closed;
  }
};

constexpr bool test_moore_fsm_v2() {
  Switch s;
  assert(s.is_in_state<ClosedState>());
  assert(s.state_tag() == SwitchStateTag::closed);
  assert(s.closed_entries == 1);
  assert(s.visit(SwitchVisitor{}) == SwitchStateTag::closed);

  s.force_transition<OpenState>();
  assert(s.is_in_state<OpenState>());
  assert(s.state_tag() == SwitchStateTag::open);
  assert(s.closed_entries == 1);
  assert(s.open_entries == 1);
  assert(s.visit(SwitchVisitor{}) == SwitchStateTag::open);

  s.dispatch(OpenEvent{});
  assert(s.is_in_state<OpenState>());
  assert(s.state_tag() == SwitchStateTag::open);
  assert(s.closed_entries == 1);
  assert(s.open_entries == 1);
  assert(s.visit(SwitchVisitor{}) == SwitchStateTag::open);

  s.dispatch(CloseEvent{});
  assert(s.is_in_state<ClosedState>());
  assert(s.state_tag() == SwitchStateTag::closed);
  assert(s.closed_entries == 2);
  assert(s.open_entries == 1);
  assert(s.visit(SwitchVisitor{}) == SwitchStateTag::closed);

  s.dispatch(CloseEvent{});
  assert(s.is_in_state<ClosedState>());
  assert(s.state_tag() == SwitchStateTag::closed);
  assert(s.closed_entries == 2);
  assert(s.open_entries == 1);
  assert(s.visit(SwitchVisitor{}) == SwitchStateTag::closed);

  s.dispatch(OpenEvent{});
  assert(s.is_in_state<OpenState>());
  assert(s.state_tag() == SwitchStateTag::open);
  assert(s.closed_entries == 2);
  assert(s.open_entries == 2);
  assert(s.visit(SwitchVisitor{}) == SwitchStateTag::open);

  return true;
}

static_assert(test_moore_fsm_v2());

} // namespace moore_fsm

namespace mealy_fsm {

class Switch;

struct OpenEvent {};

struct CloseEvent {};

struct UpdateEvent {};

enum class SwitchStateTag { open, closed };

struct OpenState {
  static constexpr SwitchStateTag tag{SwitchStateTag::open};
};

struct ClosedState {
  static constexpr SwitchStateTag tag{SwitchStateTag::closed};
};

struct Transitions {
  static constexpr auto on_event(Switch&, OpenState const&, OpenEvent const&);
  static constexpr auto on_event(Switch&, OpenState const&, CloseEvent const&);
  static constexpr auto on_event(Switch&, ClosedState const&, OpenEvent const&);
  static constexpr auto on_event(
      Switch&,
      ClosedState const&,
      CloseEvent const&
  );
};

class Switch : public emb::fsm::v2::finite_state_machine<
                   Switch,
                   emb::fsm::v2::mealy_policy,
                   Transitions,
                   OpenState,
                   ClosedState> {
public:
  constexpr Switch() : fsm_type(ClosedState{}) {
    start_fsm();
  }

  constexpr ~Switch() {
    stop_fsm();
  }
};

constexpr auto Transitions::on_event(
    Switch& s,
    OpenState const&,
    OpenEvent const&
) {
  return std::nullopt;
}

constexpr auto Transitions::on_event(
    Switch& s,
    OpenState const&,
    CloseEvent const&
) {
  return ClosedState{};
}

constexpr auto Transitions::on_event(
    Switch& s,
    ClosedState const&,
    OpenEvent const&
) {
  return OpenState{};
}

constexpr auto Transitions::on_event(
    Switch& s,
    ClosedState const&,
    CloseEvent const&
) {
  return std::nullopt;
}

struct SwitchVisitor {
  constexpr SwitchStateTag operator()(OpenState const&) {
    return SwitchStateTag::open;
  }

  constexpr SwitchStateTag operator()(ClosedState const&) {
    return SwitchStateTag::closed;
  }
};

constexpr bool test_mealy_fsm_v2() {
  Switch s;
  assert(s.is_in_state<ClosedState>());
  assert(s.state_tag() == SwitchStateTag::closed);
  assert(s.visit(SwitchVisitor{}) == SwitchStateTag::closed);

  s.force_transition<OpenState>();
  assert(s.is_in_state<OpenState>());
  assert(s.state_tag() == SwitchStateTag::open);
  assert(s.visit(SwitchVisitor{}) == SwitchStateTag::open);

  s.dispatch(OpenEvent{});
  assert(s.is_in_state<OpenState>());
  assert(s.state_tag() == SwitchStateTag::open);
  assert(s.visit(SwitchVisitor{}) == SwitchStateTag::open);

  s.dispatch(CloseEvent{});
  assert(s.is_in_state<ClosedState>());
  assert(s.state_tag() == SwitchStateTag::closed);
  assert(s.visit(SwitchVisitor{}) == SwitchStateTag::closed);

  s.dispatch(CloseEvent{});
  assert(s.is_in_state<ClosedState>());
  assert(s.state_tag() == SwitchStateTag::closed);
  assert(s.visit(SwitchVisitor{}) == SwitchStateTag::closed);

  s.dispatch(OpenEvent{});
  assert(s.is_in_state<OpenState>());
  assert(s.state_tag() == SwitchStateTag::open);
  assert(s.visit(SwitchVisitor{}) == SwitchStateTag::open);

  return true;
}

static_assert(test_mealy_fsm_v2());

} // namespace mealy_fsm

namespace mixed_fsm {

class Switch;

struct OpenEvent {};

struct CloseEvent {};

struct UpdateEvent {};

struct DestroyEvent {};

enum class SwitchStateTag { open, closed, destroyed };

struct OpenState {
  static constexpr SwitchStateTag tag{SwitchStateTag::open};
};

struct ClosedState {
  static constexpr SwitchStateTag tag{SwitchStateTag::closed};
};

struct DestroyedState {
  static constexpr SwitchStateTag tag{SwitchStateTag::destroyed};
};

struct Transitions {
  static constexpr void on_entry(Switch&, OpenState const&);
  static constexpr void on_exit(Switch&, OpenState const&);
  static constexpr auto on_event(Switch&, OpenState const&, OpenEvent const&);
  static constexpr auto on_event(Switch&, OpenState const&, CloseEvent const&);
  static constexpr auto on_event(Switch&, OpenState const&, UpdateEvent const&);
  static constexpr void on_entry(Switch&, ClosedState const&);
  static constexpr void on_exit(Switch&, ClosedState const&);
  static constexpr auto on_event(Switch&, ClosedState const&, OpenEvent const&);
  static constexpr auto on_event(
      Switch&,
      ClosedState const&,
      CloseEvent const&
  );
  static constexpr auto on_event(
      Switch&,
      ClosedState const&,
      UpdateEvent const&
  );

  static constexpr void on_entry(Switch&, DestroyedState const&) {}

  static constexpr void on_exit(Switch&, DestroyedState const&) {}

  static constexpr auto on_event(
      Switch const&,
      DestroyedState const&,
      OpenEvent const&
  ) {
    return std::nullopt;
  }

  static constexpr auto on_event(
      Switch const&,
      DestroyedState const&,
      CloseEvent const&
  ) {
    return std::nullopt;
  }

  static constexpr auto on_event(
      Switch&,
      DestroyedState const&,
      UpdateEvent const&
  ) {
    return std::nullopt;
  }

  template<typename State>
  static constexpr auto on_event(
      Switch& s,
      State const& state,
      DestroyEvent const&
  ) {
    return DestroyedState{};
  }
};

class Switch : public emb::fsm::v2::finite_state_machine<
                   Switch,
                   emb::fsm::v2::mixed_policy,
                   Transitions,
                   OpenState,
                   ClosedState,
                   DestroyedState> {
public:
  constexpr Switch() : fsm_type(ClosedState{}) {
    start_fsm();
  }

  constexpr ~Switch() {
    stop_fsm();
  }

  int open_entries = 0;
  int open_exits = 0;
  int closed_entries = 0;
  int closed_exits = 0;
  int updates = 0;
};

constexpr void Transitions::on_entry(Switch& s, OpenState const&) {
  ++s.open_entries;
}

constexpr void Transitions::on_exit(Switch& s, OpenState const&) {
  ++s.open_exits;
}

constexpr auto Transitions::on_event(
    Switch& s,
    OpenState const&,
    OpenEvent const&
) {
  return std::nullopt;
}

constexpr auto Transitions::on_event(
    Switch& s,
    OpenState const&,
    CloseEvent const&
) {
  return ClosedState{};
}

constexpr auto Transitions::on_event(
    Switch& s,
    OpenState const&,
    UpdateEvent const&
) {
  ++s.updates;
  return std::nullopt;
}

constexpr void Transitions::on_entry(Switch& s, ClosedState const&) {
  ++s.closed_entries;
}

constexpr void Transitions::on_exit(Switch& s, ClosedState const&) {
  ++s.closed_exits;
}

constexpr auto Transitions::on_event(
    Switch& s,
    ClosedState const&,
    OpenEvent const&
) {
  return OpenState{};
}

constexpr auto Transitions::on_event(
    Switch& s,
    ClosedState const&,
    CloseEvent const&
) {
  return std::nullopt;
}

constexpr auto Transitions::on_event(
    Switch& s,
    ClosedState const&,
    UpdateEvent const&
) {
  ++s.updates;
  return std::nullopt;
}

struct SwitchVisitor {
  constexpr SwitchStateTag operator()(OpenState const&) {
    return SwitchStateTag::open;
  }

  constexpr SwitchStateTag operator()(ClosedState const&) {
    return SwitchStateTag::closed;
  }

  constexpr SwitchStateTag operator()(DestroyedState const&) {
    return SwitchStateTag::destroyed;
  }
};

constexpr bool test_mixed_fsm_v2() {
  Switch s;
  assert(s.is_in_state<ClosedState>());
  assert(s.state_tag() == SwitchStateTag::closed);
  assert(s.closed_entries == 1);
  assert(s.visit(SwitchVisitor{}) == SwitchStateTag::closed);

  s.dispatch(UpdateEvent{});
  assert(s.updates == 1);

  s.force_transition<OpenState>();
  assert(s.is_in_state<OpenState>());
  assert(s.state_tag() == SwitchStateTag::open);
  assert(s.closed_entries == 1);
  assert(s.closed_exits == 1);
  assert(s.open_entries == 1);
  assert(s.open_exits == 0);
  assert(s.visit(SwitchVisitor{}) == SwitchStateTag::open);

  s.dispatch(UpdateEvent{});
  assert(s.updates == 2);

  s.dispatch(OpenEvent{});
  assert(s.is_in_state<OpenState>());
  assert(s.state_tag() == SwitchStateTag::open);
  assert(s.closed_entries == 1);
  assert(s.closed_exits == 1);
  assert(s.open_entries == 1);
  assert(s.open_exits == 0);
  assert(s.visit(SwitchVisitor{}) == SwitchStateTag::open);

  s.dispatch(UpdateEvent{});
  assert(s.updates == 3);

  s.dispatch(CloseEvent{});
  assert(s.is_in_state<ClosedState>());
  assert(s.state_tag() == SwitchStateTag::closed);
  assert(s.closed_entries == 2);
  assert(s.closed_exits == 1);
  assert(s.open_entries == 1);
  assert(s.open_exits == 1);
  assert(s.visit(SwitchVisitor{}) == SwitchStateTag::closed);

  s.dispatch(UpdateEvent{});
  assert(s.updates == 4);

  s.dispatch(CloseEvent{});
  assert(s.is_in_state<ClosedState>());
  assert(s.state_tag() == SwitchStateTag::closed);
  assert(s.closed_entries == 2);
  assert(s.closed_exits == 1);
  assert(s.open_entries == 1);
  assert(s.open_exits == 1);
  assert(s.visit(SwitchVisitor{}) == SwitchStateTag::closed);

  s.dispatch(UpdateEvent{});
  assert(s.updates == 5);

  s.dispatch(OpenEvent{});
  assert(s.is_in_state<OpenState>());
  assert(s.state_tag() == SwitchStateTag::open);
  assert(s.closed_entries == 2);
  assert(s.closed_exits == 2);
  assert(s.open_entries == 2);
  assert(s.open_exits == 1);
  assert(s.visit(SwitchVisitor{}) == SwitchStateTag::open);

  s.dispatch(DestroyEvent{});
  assert(s.is_in_state<DestroyedState>());
  assert(s.state_tag() == SwitchStateTag::destroyed);
  assert(s.closed_entries == 2);
  assert(s.closed_exits == 2);
  assert(s.open_entries == 2);
  assert(s.open_exits == 2);
  assert(s.visit(SwitchVisitor{}) == SwitchStateTag::destroyed);

  return true;
}

static_assert(test_mixed_fsm_v2());

} // namespace mixed_fsm

} // namespace
