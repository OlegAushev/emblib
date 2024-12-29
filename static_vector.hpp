#pragma once

#include <emblib/core.hpp>
#include <emblib/algorithm.hpp>

namespace emb {

template <typename T, size_t Capacity>
class static_vector {
private:
    T data_[Capacity];
    size_t size_;
public:
    static_vector() : size_(0) {}

    explicit static_vector(size_t size) : size_(size) {
        assert(size <= Capacity);
        std::fill(begin(), end(), T());
    }

    static_vector(size_t size, const T& value) : size_(size) {
        assert(size <= Capacity);
        std::fill(begin(), end(), value);
    }

    template <class V>
    static_vector(V* first, V* last) : size_(size_t(last - first)) {
        assert(first <= last);
        assert(size_t(last - first) <= Capacity);
        std::copy(first, last, begin());
    }
public:
    size_t capacity() const { return Capacity; }
    size_t size() const { return size_; }
    bool empty() const { return size_ == 0; }
    bool full() const { return size_ == Capacity; }

    T& operator[] (size_t pos) {
#ifdef NDEBUG
        return data_[pos];
#else
        return at(pos);
#endif
    }

    const T& operator[](size_t pos) const {
#ifdef NDEBUG
        return data_[pos];
#else
        return at(pos);
#endif
    }

    T& at(size_t pos) {
        assert(pos < size_);
        return data_[pos];
    }

    const T& at(size_t pos) const {
        assert(pos < size_);
        return data_[pos];
    }
public:
    T* begin() { return data_; }
    T* end() { return data_ + size_; }
    const T* begin() const { return data_; }
    const T* end() const { return data_ + size_; }

    T* data() { return data_; }
    const T* data() const { return data_; }

    T& front() {
        assert(!empty());
        return data_[0];
    }

    const T& front() const {
        assert(!empty());
        return data_[0];
    }

    T& back() {
        assert(!empty());
        return data_[size_ - 1];
    }

    const T& back() const {
        assert(!empty());
        return data_[size_ - 1];
    }
public:
    void resize(size_t size) {
        assert(size <= Capacity);
        if (size > size_) {
            std::fill(data_ + size_, data_ + size, T());
        }
        size_ = size;
    }

    void resize(size_t size, const T& value) {
        assert(size <= Capacity);
        if (size > size_) {
            std::fill(data_ + size_, data_ + size, value);
        }
        size_ = size;
    }

    void clear() {
        size_ = 0;
    }
public:
    void push_back(const T& value) {
        assert(!full());
        data_[size_++] = value;
    }

    void pop_back() {
        assert(!empty());
        --size_;
    }

    void insert(T* pos, const T& value) {
        assert(!full());
        assert(pos <= end());

        static_vector<T, Capacity> buf(pos, end());
        *pos++ = value;
        std::copy(buf.begin(), buf.end(), pos);
        ++size_;
    }

    void insert(T* pos, size_t count, const T& value) {
        assert((size_ + count) <= Capacity);
        assert(pos <= end());

        static_vector<T, Capacity> buf(pos, end());
        for (size_t i = 0; i < count; ++i) {
            *pos++ = value;
        }
        std::copy(buf.begin(), buf.end(), pos);
        size_ += count;
    }

    template <class V>
    void insert(T* pos, V* first, V* last) {
        assert(first <= last);
        assert(size_t(size_ + last - first) <= Capacity);
        assert(pos <= end());

        static_vector<T, Capacity> buf(pos, end());
        pos = std::copy(first, last, pos);
        std::copy(buf.begin(), buf.end(), pos);
        size_ = size_ + size_t(last - first);
    }
public:
    T* erase(T* pos) {
        assert(!empty());
        assert(pos < end());

        static_vector<T, Capacity> buf(pos + 1, end());
        --size_;
        return std::copy(buf.begin(), buf.end(), pos);
    }

    T* erase(T* first, T* last) {
        assert(first <= last);
        assert(size_ >= size_t(last - first));
        assert(begin() <= first);
        assert(last <= end());

        static_vector<T, Capacity> buf(last, end());
        size_ = size_ - size_t(last - first);
        return std::copy(buf.begin(), buf.end(), first);
    }
};

} // namespace emb
