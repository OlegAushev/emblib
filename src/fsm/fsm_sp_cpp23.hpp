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

//

} // namespace sp2

} // namespace fsm
} // namespace emb

#endif
