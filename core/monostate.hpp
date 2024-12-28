#pragma once

#if __cplusplus >= 201100
#include <cstdint>
#else
#include <stdint.h>
#endif
#include <cassert>
#include <cstddef>

namespace emb {

#if __cplusplus >= 201100

template<class Derived>
class monostate {
private:
    static inline bool initialized_{false};
protected:
    monostate() { assert(initialized_); }

    ~monostate() = default;

    static void init_monostate() {
        assert(!initialized_);
        initialized_ = true;
    }
public:
    static bool initialized() { return initialized_; }
};

#else

template<class Derived>
class monostate {
private:
    static bool initialized_;
protected:
    monostate() { assert(initialized_); }

    ~monostate() {}

    static void init_monostate() {
        assert(!initialized_);
        initialized_ = true;
    }
public:
    static bool initialized() { return initialized_; }
};

template<class Derived>
bool monostate<Derived>::initialized_ = false;

#endif

} // namespace emb
