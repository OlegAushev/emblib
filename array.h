#pragma once


#include <emblib/core.h>
#if defined(EMBLIB_ARM)
#include <array>
#endif


namespace emb {


#if defined(EMBLIB_C28X)

template <typename T, size_t Size>
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

    const T& operator[](size_t pos) const {
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

    const T& at(size_t pos) const {
        assert(pos < Size);
        return data[pos];
    }

    T* begin() { return data; }
    T* end() { return data + Size; }
    const T* begin() const { return data; }
    const T* end() const { return data + Size; }

    void fill(const T& value) {
        for (size_t i = 0; i < Size; ++i) {
            data[i] = value;
        }
    }
};

#elif defined(EMBLIB_ARM)

template <typename T, size_t Size>
using array = std::array<T, Size>;

#endif


} // namespace emb
