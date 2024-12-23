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


class noncopyable {
protected:
    noncopyable() {}
    ~noncopyable() {}
private:
    noncopyable(const noncopyable&);
    const noncopyable& operator=(const noncopyable&);
};


#elif defined(EMBLIB_ARM)


class noncopyable {
public:
    noncopyable() = default;
    noncopyable(const noncopyable& other) = delete;
    noncopyable& operator=(const noncopyable& other) = delete;
    virtual ~noncopyable() = default;
};


#endif


} // namespace emb
