#pragma once

#if __cplusplus >= 201100
#include <cstdint>
#else
#include <stdint.h>
#endif
#include <cstddef>
#include <cassert>

namespace emb {

#if __cplusplus >= 201100

template <class T>
class monostate {
private:
    static inline bool initialized_ = false;
protected:
    monostate() {
        assert(initialized_);
    }

    ~monostate() = default;

    static void set_initialized() {
        assert(!initialized_);
        initialized_ = true;
    }
public:
    static bool initialized() { return initialized_; }
};

#else

template <class T>
class monostate {
private:
    static bool initialized_;
protected:
    monostate() {
        assert(initialized_);
    }

    ~monostate() {}

    static void set_initialized() {
        assert(!initialized_);
        initialized_ = true;
    }
public:
    static bool initialized() { return initialized_; }
};

template <class T>
bool monostate<T>::initialized_ = false;

#endif

} // namespace emb
