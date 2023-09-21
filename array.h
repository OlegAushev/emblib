#pragma once


#include <emblib_c28x/core.h>


namespace emb {


template <typename T, int Size>
class array {
public:
    T data[Size];

    int size() const { return Size; }

    T& operator[] (int pos) {
#ifdef NDEBUG
        return data[pos];
#else
        return at(pos);
#endif
    }

    const T& operator[](int pos) const {
#ifdef NDEBUG
        return data[pos];
#else
        return at(pos);
#endif
    }

    T& at(int pos) {
        assert((pos >= 0) && (pos < Size));
        return data[pos];
    }

    const T& at(int pos) const {
        assert((pos >= 0) && (pos < Size));
        return data[pos];
    }

    T* begin() { return data; }
    T* end() { return data + Size; }
    const T* begin() const { return data; }
    const T* end() const { return data + Size; }

    void fill(const T& value) {
        for (int i = 0; i < Size; ++i) {
            data[i] = value;
        }
    }
};


} // namespace emb
