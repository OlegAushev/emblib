#pragma once


#if defined(EMBLIB_C28X)
#include <stdint.h>
#include <stddef.h>
#include <assert.h>
#elif defined(EMBLIB_ARM)
#include <cstdint>
#include <cstddef>
#include <cassert>
#endif


namespace emb {


#if defined(EMBLIB_C28X)


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


#elif defined(EMBLIB_ARM)


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


#endif


} // namespace emb
