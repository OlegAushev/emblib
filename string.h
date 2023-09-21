#pragma once


#include <emblib/core.h>
#include <emblib/algorithm.h>
#include <cstring>


namespace emb {


template <int Capacity>
class string {
private:
    static const int _data_len = Capacity + 1;
    char _data[_data_len];
    int _len;
public:
    string() {
        memset(_data, 0, _data_len);
        _len = 0;
    }

    string(const char str[]) {
        memset(_data, 0, _data_len);
        strncpy(_data, str, Capacity);
        _len = strlen(str);
    }
public:
    int capacity() const { return Capacity; }
    int lenght() const { return _len; }
    int size() const { return _len; }
    bool empty() const { return _len == 0; }
    bool full() const { return _len == Capacity; }

    char& operator[] (int pos) {
#ifdef NDEBUG
        return _data[pos];
#else
        return at(pos);
#endif
    }

    const char& operator[](int pos) const {
#ifdef NDEBUG
        return _data[pos];
#else
        return at(pos);
#endif
    }

    char& at(int pos) {
        assert(pos < _len);
        return _data[pos];
    }

    const char& at(int pos) const {
        assert(pos < _len);
        return _data[pos];
    }
public:
    char* begin() { return _data; }
    char* end() { return _data + _len; }
    const char* begin() const { return _data; }
    const char* end() const { return _data + _len; }

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
        return _data[_len - 1];
    }

    const char& back() const {
        assert(!empty());
        return _data[_len - 1];
    }
public:
    void resize(int len) {
        assert(len <= Capacity);
        if (len > _len) {
            emb::fill(_data + _len, _data + len, 0);
        } else {
            emb::fill(_data + len, _data + _len, 0);
        }
        _len = len;
    }

    void resize(int len, char ch) {
        assert(len <= Capacity);
        if (len > _len) {
            emb::fill(_data + _len, _data + len, ch);
        } else {
            emb::fill(_data + len, _data + _len, ch);
        }
        _len = len;
    }

    void clear() {
        memset(_data, 0, _data_len);
        _len = 0;
    }
public:
    void push_back(char ch) {
        assert(!full());
        _data[_len++] = ch;
    }

    void pop_back() {
        assert(!empty());
        _data[--_len] = 0;
    }
public:
    void insert(int index, char ch) {
        assert(!full());
        assert(index <= lenght());

        if (index == _len) {
            push_back(ch);
            return;
        }

        memmove(_data + index + 1 , _data + index, _len - index);
        _data[index] = ch;
        ++_len;
    }
};


template <int Capacity>
inline bool operator==(const string<Capacity>& lhs, const string<Capacity>& rhs) {
    return strcmp(lhs.data(), rhs.data()) == 0;
}


template <int Capacity>
inline bool operator!=(const string<Capacity>& lhs, const string<Capacity>& rhs) {
    return !(lhs == rhs);
}


} // namespace emb
