#pragma once

#include <emblib/core.hpp>
#include <emblib/algorithm.hpp>
#include <cstring>

namespace emb {

template <size_t Capacity>
class static_string {
private:
    char _data[Capacity];
    size_t _size; // includes terminating null
public:
    static_string() {
        memset(_data, 0, Capacity);
        _size = 1;
    }

    static_string(const char* str) {
        memset(_data, 0, Capacity);
        strncpy(_data, str, Capacity-1);
        _size = strlen(str)+1;
    }
public:
    size_t capacity() const { return Capacity; }
    size_t lenght() const { return _size - 1; }
    size_t size() const { return _size - 1; }
    bool empty() const { return _size == 1; }
    bool full() const { return _size == Capacity; }

    char& operator[] (size_t pos) {
#ifdef NDEBUG
        return _data[pos];
#else
        return at(pos);
#endif
    }

    const char& operator[](size_t pos) const {
#ifdef NDEBUG
        return _data[pos];
#else
        return at(pos);
#endif
    }

    char& at(size_t pos) {
        assert(pos < size());
        return _data[pos];
    }

    const char& at(size_t pos) const {
        assert(pos < size());
        return _data[pos];
    }
public:
    char* begin() { return _data; }
    char* end() { return _data + size(); }
    const char* begin() const { return _data; }
    const char* end() const { return _data + size(); }

    char* data() { return _data; }
    const char* data() const { return _data; }

    char& front() {
        assert(!empty());
        return _data[0];
    }

    const char& front() const {
        assert(!empty());
        return _data[0];
    }

    char& back() {
        assert(!empty());
        return _data[size() - 1];
    }

    const char& back() const {
        assert(!empty());
        return _data[size() - 1];
    }
public:
    void resize(size_t len) {
        assert(len < Capacity);
        if (len > size()) {
            emb::fill(end(), _data + len, 0);
        } else {
            emb::fill(_data + len, end(), 0);
        }
        _size = len + 1;
    }

    void resize(size_t len, char ch) {
        assert(len < Capacity);
        if (len > size()) {
            emb::fill(end(), _data + len, ch);
        } else {
            emb::fill(_data + len, end(), ch);
        }
        _size = len + 1;
    }

    void clear() {
        memset(_data, 0, Capacity);
        _size = 1;
    }
public:
    void push_back(char ch) {
        assert(!full());
        _data[size()] = ch;
        ++_size;
    }

    void pop_back() {
        assert(!empty());
        _data[size()-1] = 0;
        --_size;
    }
public:
    void insert(size_t index, char ch) {
        assert(!full());
        assert(index <= size());

        if (index == size()) {
            push_back(ch);
            return;
        }

        memmove(_data + index + 1 , _data + index, size() - index);
        _data[index] = ch;
        ++_size;
    }
};

template <size_t Capacity>
inline bool operator==(const static_string<Capacity>& lhs,
                       const static_string<Capacity>& rhs) {
    return strcmp(lhs.data(), rhs.data()) == 0;
}

template <size_t Capacity>
inline bool operator!=(const static_string<Capacity>& lhs,
                       const static_string<Capacity>& rhs) {
    return !(lhs == rhs);
}

} // namespace emb
