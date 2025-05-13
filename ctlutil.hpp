#pragma once

#include <emblib/core.hpp>
#include <emblib/stack.hpp>

namespace emb {
namespace ctl {

#if __cplusplus > 201100

template<typename ConcreteControl>
class abstract_controllable {
private:
    ConcreteControl* control_{nullptr};
public:
    abstract_controllable() = default;
protected:
    ~abstract_controllable() = default;
public:
    ConcreteControl* control_item() const { return control_; }

    void grant_control(ConcreteControl* control) {
        if (control == control_) {
            return;
        }

        ConcreteControl* old_control = control_; // make copy, critical section
        if (old_control) {
            old_control->release_control();
        }

        control_ = control;
        if (control) {
            control->take_control(this);
        }
    }

    void revoke_control() {
        ConcreteControl control = control_; // make copy, critical section
        if (control) {
            control->release_control;
        }
        control_ = nullptr;
    }
public:
    virtual void visit(ConcreteControl& control) = 0;

    void visit() {
        if (control_) {
            visit(*control_);
        }
    }
};

template<typename ConcreteControl>
class abstract_control {
    friend class abstract_controllable<ConcreteControl>;
private:
    abstract_controllable<ConcreteControl>* obj_{nullptr};
public:
    abstract_control() = default;
protected:
    ~abstract_control() = default;
private:
    void take_control(abstract_controllable<ConcreteControl>* obj) {
        obj_ = obj;
        visit();
    }

    void release_control() { obj_ = nullptr; }
protected:
    void visit() {
        // make copy, critical section
        abstract_controllable<ConcreteControl>* obj = obj_;
        if (obj) {
            obj->visit(*static_cast<ConcreteControl*>(this));
        }
    }
};

#elif __cplusplus < 201100

template<typename ConcreteControl>
class abstract_controllable {
private:
    ConcreteControl* control_;
public:
    abstract_controllable() : control_(NULL) {}

    ConcreteControl* control_item() const { return control_; }

    void grant_control(ConcreteControl* control) {
        if (control == control_) {
            return;
        }

        ConcreteControl* old_control = control_; // make copy, critical section
        if (old_control) {
            old_control->release_control();
        }

        control_ = control;
        if (control) {
            control->take_control(this);
        }
    }

    void revoke_control() {
        ConcreteControl control = control_; // make copy, critical section
        if (control) {
            control->release_control;
        }
        control_ = NULL;
    }
public:
    virtual void visit(ConcreteControl& control) = 0;

    void visit() {
        if (control_) {
            visit(*control_);
        }
    }
};

template<typename ConcreteControl>
class abstract_control {
    friend class abstract_controllable<ConcreteControl>;
private:
    abstract_controllable<ConcreteControl>* obj_;
public:
    abstract_control() : obj_(NULL) {}
private:
    void take_control(abstract_controllable<ConcreteControl>* obj) {
        obj_ = obj;
        visit();
    }

    void release_control() { obj_ = NULL; }
protected:
    void visit() {
        // make copy, critical section
        abstract_controllable<ConcreteControl>* obj = obj_;
        if (obj) {
            obj->visit(*static_cast<ConcreteControl*>(this));
        }
    }
};

#endif

} // namespace ctl
} // namespace emb
