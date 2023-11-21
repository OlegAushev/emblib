#pragma once


#include <emblib/core.h>
#include <emblib/algorithm.h>


namespace emb {


template <typename T, size_t Capacity>
class static_vector {
private:
    T _data[Capacity];
    size_t _size;
public:
    static_vector() : _size(0) {}

    explicit static_vector(size_t size) : _size(size) {
        assert(size <= Capacity);
        emb::fill(begin(), end(), T());
    }

    static_vector(size_t size, const T& value) : _size(size) {
        assert(size <= Capacity);
        emb::fill(begin(), end(), value);
    }

    template <class V>
    static_vector(V* first, V* last) : _size(size_t(last - first)) {
        assert(first <= last);
        assert((last - first) <= Capacity);
        emb::copy(first, last, begin());
    }
public:
    size_t capacity() const { return Capacity; }
    size_t size() const { return _size; }
    bool empty() const { return _size == 0; }
    bool full() const { return _size == Capacity; }

    T& operator[] (size_t pos) {
#ifdef NDEBUG
        return _data[pos];
#else
        return at(pos);
#endif
    }

    const T& operator[](size_t pos) const {
#ifdef NDEBUG
        return _data[pos];
#else
        return at(pos);
#endif
    }

    T& at(size_t pos) {
        assert(pos < _size);
        return _data[pos];
    }

    const T& at(size_t pos) const {
        assert((pos >= 0) && (pos < _size));
        return _data[pos];
    }
public:
    T* begin() { return _data; }
    T* end() { return _data + _size; }
    const T* begin() const { return _data; }
    const T* end() const { return _data + _size; }

    T* data() { return _data; }
    const T* data() const { return _data; }

    T& front() {
        assert(!empty());
        return _data[0];
    }

    const T& front() const {
        assert(!empty());
        return _data[0];
    }

    T& back() {
        assert(!empty());
        return _data[_size - 1];
    }

    const T& back() const {
        assert(!empty());
        return _data[_size - 1];
    }
public:
    void resize(size_t size) {
        assert(size <= Capacity);
        if (size > _size) {
            emb::fill(_data + _size, _data + size, T());
        }
        _size = size;
    }

    void resize(size_t size, const T& value) {
        assert(size <= Capacity);
        if (size > _size) {
            emb::fill(_data + _size, _data + size, value);
        }
        _size = size;
    }

    void clear() {
        _size = 0;
    }
public:
    void push_back(const T& value) {
        assert(!full());
        _data[_size++] = value;
    }

    void pop_back() {
        assert(!empty());
        --_size;
    }

    void insert(T* pos, const T& value) {
        assert(!full());
        assert(pos <= end());

        static_vector<T, Capacity> buf(pos, end());
        *pos++ = value;
        emb::copy(buf.begin(), buf.end(), pos);
        ++_size;
    }

    void insert(T* pos, size_t count, const T& value) {
        assert((_size + count) <= Capacity);
        assert(pos <= end());

        static_vector<T, Capacity> buf(pos, end());
        for (size_t i = 0; i < count; ++i) {
            *pos++ = value;
        }
        emb::copy(buf.begin(), buf.end(), pos);
        _size += count;
    }

    template <class V>
    void insert(T* pos, V* first, V* last) {
        assert(first <= last);
        assert((_size + last - first) <= Capacity);
        assert(pos <= end());

        static_vector<T, Capacity> buf(pos, end());
        pos = emb::copy(first, last, pos);
        emb::copy(buf.begin(), buf.end(), pos);
        _size = _size + size_t(last - first);
    }
public:
    T* erase(T* pos) {
        assert(!empty());
        assert(pos < end());

        static_vector<T, Capacity> buf(pos + 1, end());
        --_size;
        return emb::copy(buf.begin(), buf.end(), pos);
    }

    T* erase(T* first, T* last) {
        assert(_size >= (last - first));
        assert(begin() <= first);
        assert(first <= last);
        assert(last <= end());

        static_vector<T, Capacity> buf(last, end());
        _size = _size - size_t(last - first);
        return emb::copy(buf.begin(), buf.end(), first);
    }
};


} // namespace emb
