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

class noncopyable {
public:
    noncopyable() = default;
    noncopyable(const noncopyable& other) = delete;
    noncopyable& operator=(const noncopyable& other) = delete;
    virtual ~noncopyable() = default;
};

#else

class noncopyable {
protected:
    noncopyable() {}
    ~noncopyable() {}
private:
    noncopyable(const noncopyable&);
    const noncopyable& operator=(const noncopyable&);
};

#endif

} // namespace emb
