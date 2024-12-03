#pragma once

#if defined(EMBLIB_C28X)

#include <emblib/array.h>
#include <emblib/chrono.h>

namespace emb {
namespace fsm {

template<typename Object, typename State>
class abstract_state {
private:
    const State id_;
    emb::chrono::milliseconds enter_timepoint_;
protected:
    abstract_state(State id)
            : id_(id),
              enter_timepoint_(emb::chrono::steady_clock::now()) {}
    static void change_state(Object* object, State state) {
        const State prev_state = object->state();
        const State next_state = state;
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
    emb::chrono::milliseconds time_since_enter() const {
        return emb::chrono::steady_clock::now() - enter_timepoint_;
    }
};

template<typename State, typename AbstractState, size_t StateCount>
class abstract_object {
private:
    emb::array<AbstractState*, StateCount> states_;
    State state_id_;
protected:
    AbstractState* state_;
    void change_state(State state) {
        state_ = states_[state.underlying_value()];
        state_id_ = state_->id();
    }
public:
    abstract_object(State init_state) {
        for (size_t i = 0; i < StateCount; ++i) {
            states_[i] = AbstractState::create(State(i));
        }
        change_state(init_state);
    }

    virtual ~abstract_object() {
        for (size_t i = 0; i < StateCount; ++i) {
            AbstractState::destroy(State(i), states_[i]);
        }
    }

    State state() const {
        return state_id_;
    }
};

} // namespace fsm
} // namespace emb

#endif
