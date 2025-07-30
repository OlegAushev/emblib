#pragma once

#include <emb/core.hpp>
#if __cplusplus >= 201100
#include <array>
#endif

namespace emb {

#if __cplusplus >= 201100

template<typename T, size_t Size>
using array = std::array<T, Size>;

#else

template<typename T, size_t Size>
class array {
public:
  T data[Size];

  size_t size() const { return Size; }

  T& operator[](size_t pos) {
#ifdef NDEBUG
    return data[pos];
#else
    return at(pos);
#endif
  }

  T const& operator[](size_t pos) const {
#ifdef NDEBUG
    return data[pos];
#else
    return at(pos);
#endif
  }

  T& at(size_t pos) {
    assert(pos < Size);
    return data[pos];
  }

  T const& at(size_t pos) const {
    assert(pos < Size);
    return data[pos];
  }

  T* begin() { return data; }

  T* end() { return data + Size; }

  T const* begin() const { return data; }

  T const* end() const { return data + Size; }

  void fill(T const& value) {
    for (size_t i = 0; i < Size; ++i) {
      data[i] = value;
    }
  }
};

#endif

} // namespace emb
