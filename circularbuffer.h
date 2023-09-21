#pragma once


#include <emblib_c28x/core.h>


namespace emb {


template <typename T, int Capacity>
class circular_buffer {
private:
    T _data[Capacity];
    int _front;
    int _back;
    bool _full;
public:
    circular_buffer()
            : _front(0)
            , _back(0)
            , _full(false) {
    }

    void clear() {
        _front = 0;
        _back = 0;
        _full = false;
    }

    bool empty() const { return (!_full && (_front == _back)); }
    bool full() const { return _full; }
    int capacity() const { return Capacity; }

    int size() const {
        int size = Capacity;
        if (!_full) {
            if (_back >= _front) {
                size = _back - _front;
            } else {
                size = Capacity + _back - _front;
            }
        }
        return size;
    }

    void push(const T& value) {
        _data[_back] = value;
        if (_full) {
            _front = (_front + 1) % Capacity;
        }
        _back = (_back + 1) % Capacity;
        _full = (_front == _back);
    }

    const T& front() const {
        assert(!empty());
        return _data[_front];
    }

    const T& back() const {
        assert(!empty());
        return _data[(_back + Capacity - 1) % Capacity];
    }

    void pop() {
        assert(!empty());
        _full = false;
        _front = (_front + 1) % Capacity;
    }

    const T* data() const { return _data; }
    const T* begin() const { return _data; }
    const T* end() const { return _data + Capacity; }

    void fill(const T& value) {
        for (int i = 0; i < Capacity; ++i) {
            _data[i] = value;
        }
    }
};


} // namespace emb

