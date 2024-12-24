#pragma once

#if defined(EMBLIB_C28X)
#include <stdint.h>
#include <stddef.h>
#include <assert.h>
#else
#include <cstdint>
#include <cstddef>
#include <cassert>
#endif

namespace emb {

#if __cplusplus >= 201100

template <class T>
class singleton {
private:
    static inline T* instance_ = nullptr;
    static inline bool initialized_ = false;
protected:
    singleton(T* self) {
        assert(!initialized_);
        instance_ = self;
        initialized_ = true;
    }
public:
    static T* instance() {
        assert(initialized_);
        return instance_;
    }

    static bool initialized() { return initialized_; }

    virtual ~singleton() {
        initialized_ = false;
        instance_ = nullptr;
    }
};

template <class T, size_t Size>
class singleton_array
{
private:
    static inline T* instance_[Size];
    static inline bool initialized_[Size];
    static inline bool constructed_ = false;
protected:
    singleton_array(T* self, size_t instance_idx) {
        assert(instance_idx < Size);
        assert(!initialized_[instance_idx]);
        if (!constructed_) {
            for (size_t i = 0; i < Size; ++i) {
                instance_[i] = nullptr;
                initialized_[i] = false;
            }
            constructed_ = true;
        }

        instance_[instance_idx] = self;
        initialized_[instance_idx] = true;
    }
public:
    static T* instance(size_t instance_idx) {
        assert(constructed_);
        assert(instance_idx < Size);
        assert(initialized_[instance_idx]);
        return instance_[instance_idx];
    }

    static bool initialized(size_t instance_idx) {
        assert(instance_idx < Size);
        if (!constructed_) return false;
        return initialized_[instance_idx];
    }
};

#else

template <class T>
class singleton {
private:
    static T* instance_;
    static bool initialized_;
protected:
    singleton(T* self) {
        register_object(self);
    }

    ~singleton() {
        deregister_object();
    }
public:
    static T* instance() {
        assert(initialized_);
        return instance_;
    }

    static bool initialized() { return initialized_; }

    static void register_object(T* obj) {
        assert(!initialized_);
        instance_ = obj;
        initialized_ = true;
    }

    static void deregister_object() {
        initialized_ = false;
        instance_ = static_cast<T*>(NULL);
    }
};

template <class T>
T* singleton<T>::instance_ = static_cast<T*>(NULL);
template <class T>
bool singleton<T>::initialized_ = false;


template <class T, size_t Size>
class singleton_array {
private:
    static T* instance_[Size];
    static bool initialized_[Size];
    static bool constructed_;
protected:
    singleton_array(T* self, size_t instance_idx) {
        assert(instance_idx < Size);
        assert(!initialized_[instance_idx]);
        if (!constructed_) {
            for (size_t i = 0; i < Size; ++i) {
                instance_[i] = static_cast<T*>(NULL);
                initialized_[i] = false;
            }
            constructed_ = true;
        }

        instance_[instance_idx] = self;
        initialized_[instance_idx] = true;
    }
public:
    static T* instance(size_t instance_idx) {
        assert(constructed_);
        assert(instance_idx < Size);
        assert(initialized_[instance_idx]);
        return instance_[instance_idx];
    }

    static bool initialized(size_t instance_idx) {
        assert(instance_idx < Size);
        if (!constructed_) { return false; }
        return initialized_[instance_idx];
    }
};

template <class T, size_t Size>
T* singleton_array<T, Size>::instance_[Size];
template <class T, size_t Size>
bool singleton_array<T, Size>::initialized_[Size];
template <class T, size_t Size>
bool singleton_array<T, Size>::constructed_ = false;

#endif

} // namespace emb
