#pragma once

#if __cplusplus >= 202300

#include <emblib/chrono.hpp>

#include <array>
#include <type_traits>
#include <utility>

namespace emb {
namespace fsm {

template<typename Object, typename State, typename LockGuard = void*>
class abstract_state {
    static_assert(std::is_enum_v<State>);
private:
    const State id_;
    std::chrono::milliseconds enter_timepoint_;
protected:
    abstract_state(State id)
            : id_(id), enter_timepoint_(emb::chrono::steady_clock::now()) {}
    static void change_state(Object* object, State state) {
        [[maybe_unused]] LockGuard lock_guard;
        object->current_state_->finalize(object);
        object->change_state(state);
        object->current_state_->enter_timepoint_ =
                emb::chrono::steady_clock::now();
        object->current_state_->initiate(object);
    }
    virtual void initiate(Object* object) = 0;
    virtual void finalize(Object* object) = 0;
public:
    virtual ~abstract_state() {}
    State id() const { return id_; }
    std::chrono::milliseconds time_since_enter() const {
        return emb::chrono::steady_clock::now() - enter_timepoint_;
    }
};

template<typename State, typename AbstractState, size_t StateCount>
class abstract_object {
    static_assert(std::is_enum_v<State>);
private:
    std::array<AbstractState*, StateCount> states_;
protected:
    AbstractState* current_state_;
    void change_state(State state) {
        current_state_ = states_[std::to_underlying(state)];
    }
public:
    abstract_object(State init_state) {
        for (size_t i = 0; i < StateCount; ++i) {
            states_[i] = AbstractState::create(static_cast<State>(i));
        }
        change_state(init_state);
    }
    virtual ~abstract_object() {
        for (size_t i = 0; i < StateCount; ++i) {
            AbstractState::destroy(static_cast<State>(i), states_[i]);
        }
    }
    State state() const { return current_state_->id(); }
};

} // namespace fsm
} // namespace emb

#endif
