#pragma once

#if __cplusplus >= 202300

#include <emb/chrono.hpp>

#include <array>
#include <type_traits>
#include <utility>

namespace emb {
namespace fsm {

namespace sp1 {

template<typename Object, typename State, typename LockGuard = void*>
class abstract_state {
  static_assert(std::is_enum_v<State>);
private:
  State const id_;
  std::chrono::time_point<emb::chrono::steady_clock> enter_timepoint_;
protected:
  abstract_state(State id)
      : id_{id}, enter_timepoint_{emb::chrono::steady_clock::now()} {}

  static void change_state(Object* object, State state) {
    [[maybe_unused]] LockGuard lock_guard;
    State const prev_state{object->state()};
    State const next_state{state};
    object->state_->finalize(object, next_state);
    object->change_state(state);
    object->state_->enter_timepoint_ = emb::chrono::steady_clock::now();
    object->state_->initiate(object, prev_state);
  }

  virtual void initiate(Object* object, State prev_state) = 0;
  virtual void finalize(Object* object, State next_state) = 0;
public:
  virtual ~abstract_state() {}

  State id() const { return id_; }

  std::chrono::milliseconds time_since_enter() const {
    return emb::chrono::steady_clock::now() - enter_timepoint_;
  }
};

template<typename State, typename AbstractState, size_t StateNum>
class abstract_object {
  static_assert(std::is_enum_v<State>);
private:
  std::array<AbstractState*, StateNum> states_;
protected:
  AbstractState* state_;

  void change_state(State state) {
    state_ = states_[std::to_underlying(state)];
  }
public:
  abstract_object(State init_state) {
    for (auto i{0uz}; i < StateNum; ++i) {
      states_[i] = AbstractState::create(static_cast<State>(i));
    }
    change_state(init_state);
  }

  virtual ~abstract_object() {
    for (auto i{0uz}; i < StateNum; ++i) {
      AbstractState::destroy(static_cast<State>(i), states_[i]);
    }
  }

  State state() const { return state_->id(); }
};

} // namespace sp1

namespace sp2 {

template<
    typename DerivedContext,
    typename StateEnum,
    typename BaseState,
    size_t StatesNumber,
    typename LockGuard = void*>
class abstract_fsm {
private:
  std::array<BaseState*, StatesNumber> states_;
  BaseState* current_state_;
  std::chrono::time_point<emb::chrono::steady_clock> enter_timepoint_;
public:
  template<typename Event>
  constexpr void dispatch(Event event) {
    DerivedContext& context{static_cast<DerivedContext&>(*this)};
    if (std::optional<StateEnum> next_state{
            current_state_->dispatch(context, std::forward<Event>(event))}) {
      [[maybe_unused]] LockGuard lock_guard;
      StateEnum const prev_state{state()};
      current_state_->on_exit(context, *next_state);
      current_state_ = states_[std::to_underlying(*next_state)];
      enter_timepoint_ = emb::chrono::steady_clock::now();
      current_state_->on_enter(context, prev_state);
    }
  }
public:
  abstract_fsm(StateEnum init_state) {
    for (auto i{0uz}; i < StatesNumber; ++i) {
      states_[i] = BaseState::create(static_cast<StateEnum>(i));
    }
    current_state_ = states_[std::to_underlying(init_state)];
    enter_timepoint_ = emb::chrono::steady_clock::now();
  }

  virtual ~abstract_fsm() {
    for (auto i{0uz}; i < StatesNumber; ++i) {
      BaseState::destroy(static_cast<StateEnum>(i), states_[i]);
    }
  }

  StateEnum state() const { return current_state_->id(); }

  std::chrono::milliseconds time_since_state_enter() const {
    return emb::chrono::steady_clock::now() - enter_timepoint_;
  }
};

} // namespace sp2

} // namespace fsm
} // namespace emb

#endif
