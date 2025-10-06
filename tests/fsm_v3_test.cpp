#ifdef __cpp_constexpr

#include "tests.hpp"

#include <emb/fsm.hpp>

namespace emb {
namespace internal {
namespace tests {

namespace moore_fsm {

class Switch;

struct OpenEvent {};

struct CloseEvent {};

struct UpdateEvent {};

enum class SwitchStateId { open, closed };

struct OpenState {
  static constexpr SwitchStateId id{SwitchStateId::open};
  static constexpr void on_entry(Switch&);
  static constexpr auto on_event(Switch const&, OpenEvent const&);
  static constexpr auto on_event(Switch const&, CloseEvent const&);
};

struct ClosedState {
  static constexpr SwitchStateId id{SwitchStateId::closed};
  static constexpr void on_entry(Switch&);
  static constexpr auto on_event(Switch const&, OpenEvent const&);
  static constexpr auto on_event(Switch const&, CloseEvent const&);
};

class Switch : public emb::fsm::v3::finite_state_machine<
                   Switch,
                   emb::fsm::v3::moore_policy,
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

constexpr void OpenState::on_entry(Switch& s) {
  ++s.open_entries;
}

constexpr auto OpenState::on_event(Switch const& s, OpenEvent const&) {
  return std::nullopt;
}

constexpr auto OpenState::on_event(Switch const& s, CloseEvent const&) {
  return ClosedState{};
}

constexpr void ClosedState::on_entry(Switch& s) {
  ++s.closed_entries;
}

constexpr auto ClosedState::on_event(Switch const& s, OpenEvent const&) {
  return OpenState{};
}

constexpr auto ClosedState::on_event(Switch const& s, CloseEvent const&) {
  return std::nullopt;
}

struct SwitchVisitor {
  constexpr SwitchStateId operator()(OpenState const&) {
    return SwitchStateId::open;
  }

  constexpr SwitchStateId operator()(ClosedState const&) {
    return SwitchStateId::closed;
  }
};

constexpr bool test_moore_fsm_v3() {
  Switch s;
  EMB_CONSTEXPR_ASSERT(s.is_in_state<ClosedState>())
  EMB_CONSTEXPR_ASSERT(s.state_id() == SwitchStateId::closed);
  EMB_CONSTEXPR_ASSERT(s.closed_entries == 1);
  EMB_CONSTEXPR_ASSERT(s.visit(SwitchVisitor{}) == SwitchStateId::closed);

  s.force_transition<OpenState>();
  EMB_CONSTEXPR_ASSERT(s.is_in_state<OpenState>())
  EMB_CONSTEXPR_ASSERT(s.state_id() == SwitchStateId::open);
  EMB_CONSTEXPR_ASSERT(s.closed_entries == 1);
  EMB_CONSTEXPR_ASSERT(s.open_entries == 1);
  EMB_CONSTEXPR_ASSERT(s.visit(SwitchVisitor{}) == SwitchStateId::open);

  s.dispatch(OpenEvent{});
  EMB_CONSTEXPR_ASSERT(s.is_in_state<OpenState>())
  EMB_CONSTEXPR_ASSERT(s.state_id() == SwitchStateId::open);
  EMB_CONSTEXPR_ASSERT(s.closed_entries == 1);
  EMB_CONSTEXPR_ASSERT(s.open_entries == 1);
  EMB_CONSTEXPR_ASSERT(s.visit(SwitchVisitor{}) == SwitchStateId::open);

  s.dispatch(CloseEvent{});
  EMB_CONSTEXPR_ASSERT(s.is_in_state<ClosedState>())
  EMB_CONSTEXPR_ASSERT(s.state_id() == SwitchStateId::closed);
  EMB_CONSTEXPR_ASSERT(s.closed_entries == 2);
  EMB_CONSTEXPR_ASSERT(s.open_entries == 1);
  EMB_CONSTEXPR_ASSERT(s.visit(SwitchVisitor{}) == SwitchStateId::closed);

  s.dispatch(CloseEvent{});
  EMB_CONSTEXPR_ASSERT(s.is_in_state<ClosedState>())
  EMB_CONSTEXPR_ASSERT(s.state_id() == SwitchStateId::closed);
  EMB_CONSTEXPR_ASSERT(s.closed_entries == 2);
  EMB_CONSTEXPR_ASSERT(s.open_entries == 1);
  EMB_CONSTEXPR_ASSERT(s.visit(SwitchVisitor{}) == SwitchStateId::closed);

  s.dispatch(OpenEvent{});
  EMB_CONSTEXPR_ASSERT(s.is_in_state<OpenState>())
  EMB_CONSTEXPR_ASSERT(s.state_id() == SwitchStateId::open);
  EMB_CONSTEXPR_ASSERT(s.closed_entries == 2);
  EMB_CONSTEXPR_ASSERT(s.open_entries == 2);
  EMB_CONSTEXPR_ASSERT(s.visit(SwitchVisitor{}) == SwitchStateId::open);

  return true;
}

static_assert(test_moore_fsm_v3());

} // namespace moore_fsm

namespace mealy_fsm {

class Switch;

struct OpenEvent {};

struct CloseEvent {};

struct UpdateEvent {};

enum class SwitchStateId { open, closed };

struct OpenState {
  static constexpr SwitchStateId id{SwitchStateId::open};
  static constexpr auto on_event(Switch&, OpenEvent const&);
  static constexpr auto on_event(Switch&, CloseEvent const&);
};

struct ClosedState {
  static constexpr SwitchStateId id{SwitchStateId::closed};
  static constexpr auto on_event(Switch&, OpenEvent const&);
  static constexpr auto on_event(Switch&, CloseEvent const&);
};

class Switch : public emb::fsm::v3::finite_state_machine<
                   Switch,
                   emb::fsm::v3::mealy_policy,
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

constexpr auto OpenState::on_event(Switch& s, OpenEvent const&) {
  return std::nullopt;
}

constexpr auto OpenState::on_event(Switch& s, CloseEvent const&) {
  return ClosedState{};
}

constexpr auto ClosedState::on_event(Switch& s, OpenEvent const&) {
  return OpenState{};
}

constexpr auto ClosedState::on_event(Switch& s, CloseEvent const&) {
  return std::nullopt;
}

struct SwitchVisitor {
  constexpr SwitchStateId operator()(OpenState const&) {
    return SwitchStateId::open;
  }

  constexpr SwitchStateId operator()(ClosedState const&) {
    return SwitchStateId::closed;
  }
};

constexpr bool test_mealy_fsm_v3() {
  Switch s;
  EMB_CONSTEXPR_ASSERT(s.is_in_state<ClosedState>())
  EMB_CONSTEXPR_ASSERT(s.state_id() == SwitchStateId::closed);
  EMB_CONSTEXPR_ASSERT(s.visit(SwitchVisitor{}) == SwitchStateId::closed);

  s.force_transition<OpenState>();
  EMB_CONSTEXPR_ASSERT(s.is_in_state<OpenState>())
  EMB_CONSTEXPR_ASSERT(s.state_id() == SwitchStateId::open);
  EMB_CONSTEXPR_ASSERT(s.visit(SwitchVisitor{}) == SwitchStateId::open);

  s.dispatch(OpenEvent{});
  EMB_CONSTEXPR_ASSERT(s.is_in_state<OpenState>())
  EMB_CONSTEXPR_ASSERT(s.state_id() == SwitchStateId::open);
  EMB_CONSTEXPR_ASSERT(s.visit(SwitchVisitor{}) == SwitchStateId::open);

  s.dispatch(CloseEvent{});
  EMB_CONSTEXPR_ASSERT(s.is_in_state<ClosedState>())
  EMB_CONSTEXPR_ASSERT(s.state_id() == SwitchStateId::closed);
  EMB_CONSTEXPR_ASSERT(s.visit(SwitchVisitor{}) == SwitchStateId::closed);

  s.dispatch(CloseEvent{});
  EMB_CONSTEXPR_ASSERT(s.is_in_state<ClosedState>())
  EMB_CONSTEXPR_ASSERT(s.state_id() == SwitchStateId::closed);
  EMB_CONSTEXPR_ASSERT(s.visit(SwitchVisitor{}) == SwitchStateId::closed);

  s.dispatch(OpenEvent{});
  EMB_CONSTEXPR_ASSERT(s.is_in_state<OpenState>())
  EMB_CONSTEXPR_ASSERT(s.state_id() == SwitchStateId::open);
  EMB_CONSTEXPR_ASSERT(s.visit(SwitchVisitor{}) == SwitchStateId::open);

  return true;
}

static_assert(test_mealy_fsm_v3());

} // namespace moore_fsm
namespace mixed_fsm {

class Switch;

struct OpenEvent {};

struct CloseEvent {};

struct UpdateEvent {};

struct DestroyEvent {};

enum class SwitchStateId { open, closed, destroyed };

struct OpenState {
  static constexpr SwitchStateId id{SwitchStateId::open};
  static constexpr void on_entry(Switch&);
  static constexpr void on_exit(Switch&);
  static constexpr auto on_event(Switch&, OpenEvent const&);
  static constexpr auto on_event(Switch&, CloseEvent const&);
  static constexpr auto on_event(Switch&, UpdateEvent const&);
};

struct ClosedState {
  static constexpr SwitchStateId id{SwitchStateId::closed};
  static constexpr void on_entry(Switch&);
  static constexpr void on_exit(Switch&);
  static constexpr auto on_event(Switch&, OpenEvent const&);
  static constexpr auto on_event(Switch&, CloseEvent const&);
  static constexpr auto on_event(Switch&, UpdateEvent const&);
};

struct DestroyedState {
  static constexpr SwitchStateId id{SwitchStateId::destroyed};
  static constexpr void on_entry(Switch&) {}
  static constexpr void on_exit(Switch&) {}
  static constexpr auto on_event(Switch const&, OpenEvent const&) {
    return std::nullopt;
  }
  static constexpr auto on_event(Switch const&, CloseEvent const&) {
    return std::nullopt;
  }
  static constexpr auto on_event(Switch&, UpdateEvent const&) {
    return std::nullopt;
  }
};

class Switch : public emb::fsm::v3::finite_state_machine<
                   Switch,
                   emb::fsm::v3::mixed_policy,
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

constexpr void OpenState::on_entry(Switch& s) {
  ++s.open_entries;
}

constexpr void OpenState::on_exit(Switch& s) {
  ++s.open_exits;
}

constexpr auto OpenState::on_event(Switch& s, OpenEvent const&) {
  return std::nullopt;
}

constexpr auto OpenState::on_event(Switch& s, CloseEvent const&) {
  return ClosedState{};
}

constexpr auto OpenState::on_event(Switch& s, UpdateEvent const&) {
  ++s.updates;
  return std::nullopt;
}

constexpr void ClosedState::on_entry(Switch& s) {
  ++s.closed_entries;
}

constexpr void ClosedState::on_exit(Switch& s) {
  ++s.closed_exits;
}

constexpr auto ClosedState::on_event(Switch& s, OpenEvent const&) {
  return OpenState{};
}

constexpr auto ClosedState::on_event(Switch& s, CloseEvent const&) {
  return std::nullopt;
}

constexpr auto ClosedState::on_event(Switch& s, UpdateEvent const&) {
  ++s.updates;
  return std::nullopt;
}

constexpr auto on_event(Switch& s, DestroyEvent const&) {
  return DestroyedState{};
}

struct SwitchVisitor {
  constexpr SwitchStateId operator()(OpenState const&) {
    return SwitchStateId::open;
  }

  constexpr SwitchStateId operator()(ClosedState const&) {
    return SwitchStateId::closed;
  }

  constexpr SwitchStateId operator()(DestroyedState const&) {
    return SwitchStateId::destroyed;
  }
};

constexpr bool test_mixed_fsm_v3() {
  Switch s;
  EMB_CONSTEXPR_ASSERT(s.is_in_state<ClosedState>())
  EMB_CONSTEXPR_ASSERT(s.state_id() == SwitchStateId::closed);
  EMB_CONSTEXPR_ASSERT(s.closed_entries == 1);
  EMB_CONSTEXPR_ASSERT(s.visit(SwitchVisitor{}) == SwitchStateId::closed);

  s.dispatch(UpdateEvent{});
  EMB_CONSTEXPR_ASSERT(s.updates == 1);

  s.force_transition<OpenState>();
  EMB_CONSTEXPR_ASSERT(s.is_in_state<OpenState>())
  EMB_CONSTEXPR_ASSERT(s.state_id() == SwitchStateId::open);
  EMB_CONSTEXPR_ASSERT(s.closed_entries == 1);
  EMB_CONSTEXPR_ASSERT(s.closed_exits == 1);
  EMB_CONSTEXPR_ASSERT(s.open_entries == 1);
  EMB_CONSTEXPR_ASSERT(s.open_exits == 0);
  EMB_CONSTEXPR_ASSERT(s.visit(SwitchVisitor{}) == SwitchStateId::open);

  s.dispatch(UpdateEvent{});
  EMB_CONSTEXPR_ASSERT(s.updates == 2);

  s.dispatch(OpenEvent{});
  EMB_CONSTEXPR_ASSERT(s.is_in_state<OpenState>())
  EMB_CONSTEXPR_ASSERT(s.state_id() == SwitchStateId::open);
  EMB_CONSTEXPR_ASSERT(s.closed_entries == 1);
  EMB_CONSTEXPR_ASSERT(s.closed_exits == 1);
  EMB_CONSTEXPR_ASSERT(s.open_entries == 1);
  EMB_CONSTEXPR_ASSERT(s.open_exits == 0);
  EMB_CONSTEXPR_ASSERT(s.visit(SwitchVisitor{}) == SwitchStateId::open);

  s.dispatch(UpdateEvent{});
  EMB_CONSTEXPR_ASSERT(s.updates == 3);

  s.dispatch(CloseEvent{});
  EMB_CONSTEXPR_ASSERT(s.is_in_state<ClosedState>())
  EMB_CONSTEXPR_ASSERT(s.state_id() == SwitchStateId::closed);
  EMB_CONSTEXPR_ASSERT(s.closed_entries == 2);
  EMB_CONSTEXPR_ASSERT(s.closed_exits == 1);
  EMB_CONSTEXPR_ASSERT(s.open_entries == 1);
  EMB_CONSTEXPR_ASSERT(s.open_exits == 1);
  EMB_CONSTEXPR_ASSERT(s.visit(SwitchVisitor{}) == SwitchStateId::closed);

  s.dispatch(UpdateEvent{});
  EMB_CONSTEXPR_ASSERT(s.updates == 4);

  s.dispatch(CloseEvent{});
  EMB_CONSTEXPR_ASSERT(s.is_in_state<ClosedState>())
  EMB_CONSTEXPR_ASSERT(s.state_id() == SwitchStateId::closed);
  EMB_CONSTEXPR_ASSERT(s.closed_entries == 2);
  EMB_CONSTEXPR_ASSERT(s.closed_exits == 1);
  EMB_CONSTEXPR_ASSERT(s.open_entries == 1);
  EMB_CONSTEXPR_ASSERT(s.open_exits == 1);
  EMB_CONSTEXPR_ASSERT(s.visit(SwitchVisitor{}) == SwitchStateId::closed);

  s.dispatch(UpdateEvent{});
  EMB_CONSTEXPR_ASSERT(s.updates == 5);

  s.dispatch(OpenEvent{});
  EMB_CONSTEXPR_ASSERT(s.is_in_state<OpenState>())
  EMB_CONSTEXPR_ASSERT(s.state_id() == SwitchStateId::open);
  EMB_CONSTEXPR_ASSERT(s.closed_entries == 2);
  EMB_CONSTEXPR_ASSERT(s.closed_exits == 2);
  EMB_CONSTEXPR_ASSERT(s.open_entries == 2);
  EMB_CONSTEXPR_ASSERT(s.open_exits == 1);
  EMB_CONSTEXPR_ASSERT(s.visit(SwitchVisitor{}) == SwitchStateId::open);

  s.dispatch(DestroyEvent{});
  EMB_CONSTEXPR_ASSERT(s.is_in_state<DestroyedState>())
  EMB_CONSTEXPR_ASSERT(s.state_id() == SwitchStateId::destroyed);
  EMB_CONSTEXPR_ASSERT(s.closed_entries == 2);
  EMB_CONSTEXPR_ASSERT(s.closed_exits == 2);
  EMB_CONSTEXPR_ASSERT(s.open_entries == 2);
  EMB_CONSTEXPR_ASSERT(s.open_exits == 2);
  EMB_CONSTEXPR_ASSERT(s.visit(SwitchVisitor{}) == SwitchStateId::destroyed);

  return true;
}

static_assert(test_mixed_fsm_v3());

} // namespace mixed_fsm

} // namespace tests
} // namespace internal
} // namespace emb

#endif
