#pragma once


#include <emblib_c28x/core.h>


namespace emb {

template <typename T, int Capacity>
class stack {
private:
    T _data[Capacity];
    int _size;
public:
    stack() : _size(0) {}

    void clear() { _size = 0; }
    bool empty() const { return _size == 0; }
    bool full() const { return _size == Capacity; }
    int capacity() const { return Capacity; }
    int size() const { return _size; }

    void push(const T& value) {
        assert(!full());
        _data[_size] = value;
        ++_size;
    }

    const T& top() const {
        assert(!empty());
        return _data[_size-1];
    }

    void pop() {
        assert(!empty());
        --_size;
    }
};

} // namespace emb

