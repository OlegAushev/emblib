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
protected:
  noncopyable() = default;
  ~noncopyable() = default;
public:
  noncopyable(noncopyable const& other) = delete;
  noncopyable& operator=(noncopyable const& other) = delete;
};

#else

class noncopyable {
protected:
  noncopyable() {}

  ~noncopyable() {}
private:
  noncopyable(noncopyable const&);
  noncopyable const& operator=(noncopyable const&);
};

#endif

} // namespace emb
