#pragma once

#include <emblib/core.hpp>

namespace emb {

template <typename T, size_t Capacity>
class stack {
private:
    T data_[Capacity];
    size_t size_;
public:
    stack() : size_(0) {}

    void clear() { size_ = 0; }
    bool empty() const { return size_ == 0; }
    bool full() const { return size_ == Capacity; }
    size_t capacity() const { return Capacity; }
    size_t size() const { return size_; }

    void push(const T& value) {
        assert(!full());
        data_[size_] = value;
        ++size_;
    }

    const T& top() const {
        assert(!empty());
        return data_[size_-1];
    }

    void pop() {
        assert(!empty());
        --size_;
    }
};

} // namespace emb
