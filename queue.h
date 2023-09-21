#pragma once


#include <emblib_c28x/core.h>


namespace emb {


template <typename T, int Capacity>
class queue {
private:
    T _data[Capacity];
    int _front;
    int _back;
    int _size;
public:
    queue()
            : _front(0)
            , _back(0)
            , _size(0) {
    }

    void clear() {
        _front = 0;
        _back = 0;
        _size = 0;
    }

    bool empty() const { return _size == 0; }
    bool full() const { return _size == Capacity; }
    int capacity() const { return Capacity; }
    int size() const { return _size; }

    void push(const T& value) {
        assert(!full());

        if (empty()) {
            _back = _front;
        } else {
            _back = (_back + 1) % Capacity;
        }
        _data[_back] = value;
        ++_size;
    }

    const T& front() const {
        assert(!empty());
        return _data[_front];
    }

    const T& back() const {
        assert(!empty());
        return _data[_back];
    }

    void pop() {
        assert(!empty());
        _front = (_front + 1) % Capacity;
        --_size;
    }
};


} // namespace emb
