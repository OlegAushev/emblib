#pragma once

#if defined(EMBLIB_ARM)

#include <emblib/chrono.h>

#include <array>
#include <utility>

namespace emb {
namespace fsm {

template<typename Object, typename State>
class abstract_state {
private:
    const State _id;
    std::chrono::milliseconds _enter_timepoint;
protected:
    abstract_state(State id)
            : _id(id),
              _enter_timepoint(emb::chrono::steady_clock::now()) {}
    void change_state(Object* object, State state) {
        object->_current_state->_finalize(object);
        object->change_state(state);
        object->_current_state->_enter_timepoint =
            emb::chrono::steady_clock::now();
        object->_current_state->_initiate(object);
    }
    virtual void _initiate(Object* object) = 0;
    virtual void _finalize(Object* object) = 0;
public:
    virtual ~abstract_state() {}
    State id() const { return _id; }
    std::chrono::milliseconds time_since_enter() const {
        return emb::chrono::steady_clock::now() - _enter_timepoint;
    }
};

template<typename State, typename AbstractState, size_t StateCount>
class abstract_object {
private:
    std::array<AbstractState*, StateCount> _states;
protected:
    AbstractState* _current_state;
    void change_state(State state) {
        _current_state = _states[std::to_underlying(state)];
    }
public:
    abstract_object(State init_state) {
        for (size_t i = 0; i < StateCount; ++i) {
            _states[i] = AbstractState::create(static_cast<State>(i));
        }
        change_state(init_state);
    }
    virtual ~abstract_object() {
        for (size_t i = 0; i < StateCount; ++i) {
            AbstractState::destroy(static_cast<State>(i), _states[i]);
        }
    }
    State state() const { return _current_state->id(); }
};

} // namespace fsm
} // namespace emb

#endif
