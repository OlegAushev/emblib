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
class interrupt_invoker {
private:
    static T* _instance;
    static bool _initialized;
protected:
    interrupt_invoker(T* self) {
        assert(!_initialized);
        _instance = self;
        _initialized = true;
    }

    ~interrupt_invoker() {
        _initialized = false;
        _instance = static_cast<T*>(NULL);
    }
public:
    static T* instance() {
        assert(_initialized);
        return _instance;
    }

    static bool initialized() { return _initialized; }
};

template <class T>
T* interrupt_invoker<T>::_instance = static_cast<T*>(NULL);
template <class T>
bool interrupt_invoker<T>::_initialized = false;


template <class T, size_t Size>
class interrupt_invoker_array {
private:
    static T* _instance[Size];
    static bool _initialized[Size];
    static bool _constructed;
protected:
    interrupt_invoker_array(T* self, size_t instance_idx) {
        assert(instance_idx < Size);
        assert(!_initialized[instance_idx]);
        if (!_constructed) {
            for (size_t i = 0; i < Size; ++i) {
                _instance[i] = static_cast<T*>(NULL);
                _initialized[i] = false;
            }
            _constructed = true;
        }

        _instance[instance_idx] = self;
        _initialized[instance_idx] = true;
    }
public:
    static T* instance(size_t instance_idx) {
        assert(_constructed);
        assert(instance_idx < Size);
        assert(_initialized[instance_idx]);
        return _instance[instance_idx];
    }

    static bool initialized(size_t instance_idx) {
        assert(instance_idx < Size);
        if (!_constructed) { return false; }
        return _initialized[instance_idx];
    }
};

template <class T, size_t Size>
T* interrupt_invoker_array<T, Size>::_instance[Size];
template <class T, size_t Size>
bool interrupt_invoker_array<T, Size>::_initialized[Size];
template <class T, size_t Size>
bool interrupt_invoker_array<T, Size>::_constructed = false;


#elif defined(EMBLIB_ARM)


template <class T>
class interrupt_invoker {
private:
    static inline T* _instance = nullptr;
    static inline bool _initialized = false;
protected:
    interrupt_invoker(T* self) {
        assert(!_initialized);
        _instance = self;
        _initialized = true;
    }
public:
    static T* instance() {
        assert(_initialized);
        return _instance;
    }

    static bool initialized() { return _initialized; }

    virtual ~interrupt_invoker() {
        _initialized = false;
        _instance = nullptr;
    }
};


template <class T, size_t Size>
class interrupt_invoker_array
{
private:
    static inline T* _instance[Size];
    static inline bool _initialized[Size];
    static inline bool _constructed = false;
protected:
    interrupt_invoker_array(T* self, size_t instance_idx) {
        assert(instance_idx < Size);
        assert(!_initialized[instance_idx]);
        if (!_constructed) {
            for (size_t i = 0; i < Size; ++i) {
                _instance[i] = nullptr;
                _initialized[i] = false;
            }
            _constructed = true;
        }

        _instance[instance_idx] = self;
        _initialized[instance_idx] = true;
    }
public:
    static T* instance(size_t instance_idx) {
        assert(_constructed);
        assert(instance_idx < Size);
        assert(_initialized[instance_idx]);
        return _instance[instance_idx];
    }

    static bool initialized(size_t instance_idx) {
        assert(instance_idx < Size);
        if (!_constructed) return false;
        return _initialized[instance_idx];
    }
};


#endif


} // namespace emb
