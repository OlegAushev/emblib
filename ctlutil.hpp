#pragma once

#include <emblib/core.hpp>
#include <emblib/stack.hpp>

namespace emb {
namespace ctl {

#if __cplusplus < 201100

template<typename ConcreteControl>
class controllable {
public:
    virtual void visit(ConcreteControl* control) = 0;
};

template<typename ConcreteControl, size_t StackSize> class control_manager;

template<typename ConcreteControl>
class abstract_control {
    template<typename T, size_t StackSize>
    friend class control_manager;
private:
    controllable<ConcreteControl>* obj_;
public:
    abstract_control() : obj_(NULL) {}
private:
    void activate(controllable<ConcreteControl>* obj) {
        obj_ = obj;
        visit();
    }

    void deactivate() {
        obj_ = NULL;
    }
protected:
    void visit() {
        // make copy, critical section
        controllable<ConcreteControl>* obj = obj_;
        if (obj) {
            obj->visit(static_cast<ConcreteControl*>(this));
        }
    }
};

template<typename ConcreteControl, size_t StackSize>
class control_manager : public controllable<ConcreteControl> {
private:
    controllable<ConcreteControl>* obj_;
    emb::stack<ConcreteControl*, StackSize> controls_;
public:
    control_manager(controllable<ConcreteControl>* obj) : obj_(obj) {}

    void activate(ConcreteControl& control) {
        assert(!controls_.full());
        if (controls_.full()) {
            // error
            controls_.top()->deactivate();
            controls_.clear();
            return;
        }

        if (!controls_.empty()) {
            controls_.top()->deactivate();
        }

        controls_.push(&control);
        control.activate(this);
    }

    void deactivate(ConcreteControl& control) {
        assert(!controls_.empty());
        assert(controls_.top() == &control);

        if (controls_.empty()) {
            return;
        }

        if (controls_.top() != &control) {
            // error: deactivate not active control
            controls_.top()->deactivate();
            controls_.clear();
            return;
        }

        control.deactivate();
        controls_.pop();

        if (!controls_.empty()) {
            controls_.top()->activate(this);
        }
    }

    void visit(ConcreteControl* control) {
        if (controls_.top() == control) {
            obj_->visit(control);
        }
    }
};

#endif

} // namespace ctl
} // namespace emb
