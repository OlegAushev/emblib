#pragma once


#if defined(EMBLIB_C28X)
#include <stdint.h>
#include <stddef.h>
#include <assert.h>
#elif defined(EMBLIB_STM32)
#include <cstdint>
#include <cstddef>
#include <cassert>
#endif


namespace emb {


#if defined(EMBLIB_C28X)


template <class T>
class monostate {
private:
    static bool _initialized;
protected:
    monostate() {
        assert(_initialized);
    }

    ~monostate() {}

    static void set_initialized() {
        assert(!_initialized);
        _initialized = true;
    }
public:
    static bool initialized() { return _initialized; }
};

template <class T>
bool monostate<T>::_initialized = false;


#elif defined(EMBLIB_STM32)


template <class T>
class monostate {
private:
    static inline bool _initialized = false;
protected:
    monostate() {
        assert(_initialized);
    }

    ~monostate() = default;

    static void set_initialized() {
        assert(!_initialized);
        _initialized = true;
    }
public:
    static bool initialized() { return _initialized; }
};


#endif


} // namespace emb
