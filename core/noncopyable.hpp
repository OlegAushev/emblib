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
