#pragma once

#include <bitset>
#include <emb/array.hpp>
#include <emb/core.hpp>
#if defined(EMBLIB_ARM)
#include <climits>
#include <cstring>
#endif

namespace emb {

template<size_t BitCount>
class bitset {
  EMB_STATIC_ASSERT(BitCount > 0);
private:
  static size_t const _byte_count = (BitCount + CHAR_BIT - 1) / CHAR_BIT;
  static size_t const _extra_bits = BitCount % CHAR_BIT;

  unsigned int _data[_byte_count];

  static size_t _which_byte(size_t pos) { return pos / CHAR_BIT; }

  static size_t _which_bit(size_t pos) { return pos % CHAR_BIT; }

  unsigned int& _hi_byte() { return _data[_byte_count - 1]; }

  unsigned int _hi_byte() const { return _data[_byte_count - 1]; }

  static void _reset_unused_bits(unsigned int& hi_byte) {
    if (_extra_bits)
      hi_byte &= ~(~0U << _extra_bits);
  }
public:
  class Reference;

  bitset() {
    for (size_t i = 0; i < _byte_count; ++i) {
      _data[i] = 0U;
    }
  }

  bitset(emb::array<unsigned int, _byte_count> const& words) {
    for (size_t i = 0; i < _byte_count; ++i) {
      _data[i] = words[i];
    }
    _reset_unused_bits(_hi_byte());
  }

  bitset(uint64_t value) {
    EMB_STATIC_ASSERT(_byte_count <= 64 / CHAR_BIT);
    uint64_t val =
        value; // protection against implicit conversion and wrong memcpy()
    memcpy(_data, &val, _byte_count);
    _reset_unused_bits(_hi_byte());
  }

  size_t size() const { return BitCount; }

  bool operator[](size_t pos) const {
#ifdef NDEBUG
    return _data[_which_byte(pos)] & (1U << _which_bit(pos));
#else
    return test(pos);
#endif
  }

  Reference operator[](size_t pos) { return Reference(this, pos); }

  bool test(size_t pos) const {
    assert(pos < BitCount);
    return _data[_which_byte(pos)] & (1U << _which_bit(pos));
  }

  bool all() const {
    // int index is used to suppress "pointless comparison of unsigned integer with zero" warning
    for (size_t i = 0; i < _byte_count - 1; ++i) {
      if (_data[i] != 0xFFFF) {
        return false;
      }
    }

    if (_extra_bits) {
      if (_hi_byte() != ~(~0U << _extra_bits)) {
        return false;
      }
    } else if (_hi_byte() != 0xFFFF) {
      return false;
    }
    return true;
  }

  bool any() const {
    for (size_t i = 0; i < _byte_count; ++i) {
      if (_data[i] != 0) {
        return true;
      }
    }
    return false;
  }

  bool none() const { return !any(); }

  size_t count() const {
    size_t ret = 0;
    for (size_t i = 0; i < BitCount; ++i) {
      if ((*this)[i])
        ++ret;
    }
    return ret;
  }

  void set() {
    for (size_t i = 0; i < _byte_count; ++i) {
      _data[i] = 0xFFFF;
    }
    _reset_unused_bits(_hi_byte());
  }

  void set(size_t pos, bool value = true) {
    assert(pos < BitCount);
    if (value) {
      _data[_which_byte(pos)] |= 1U << _which_bit(pos);
    } else {
      _data[_which_byte(pos)] &= ~(1U << _which_bit(pos));
    }
  }

  void reset() {
    for (size_t i = 0; i < _byte_count; ++i) {
      _data[i] = 0U;
    }
  }

  void reset(size_t pos) {
    assert(pos < BitCount);
    _data[_which_byte(pos)] &= ~(1U << _which_bit(pos));
  }

  void flip() {
    for (size_t i = 0; i < _byte_count; ++i) {
      _data[i] = ~_data[i];
    }
    _reset_unused_bits(_hi_byte());
  }

  void flip(size_t pos) {
    assert(pos < BitCount);
    _data[_which_byte(pos)] ^= 1U << _which_bit(pos);
  }

  bool operator==(bitset const& rhs) const {
    for (size_t i = 0; i < _byte_count; ++i) {
      if (_data[i] != rhs._data[i]) {
        return false;
      }
    }
    return true;
  }

  bool operator!=(bitset const& rhs) const { return !((*this) == rhs); }

  struct Reference {
    bitset* self;
    size_t pos;

    Reference(bitset* self_, size_t pos_) : self(self_), pos(pos_) {}

    operator bool() const { return self->test(pos); }

    Reference& operator=(bool value) {
      self->set(pos, value);
      return *this;
    }
  };
};

} // namespace emb
