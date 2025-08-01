#pragma once

#include <algorithm>
#include <emb/algorithm.hpp>
#include <emb/circular_buffer.hpp>
#include <emb/core.hpp>
#include <emb/math.hpp>
#include <emb/units.hpp>

#ifdef EMB_CIRCULAR_BUFFER_V2
#define EMB_MOVING_AVERAGE_FILTER_V2
#endif

namespace emb {

#ifdef EMB_MOVING_AVERAGE_FILTER_V2

inline namespace v2 {

template<typename T, size_t WindowSize = 0>
  requires std::is_arithmetic_v<T> ||
           std::is_arithmetic_v<typename T::underlying_type>
class movavg_filter {
private:
  template<typename V, bool B>
  struct get_arithmetic_type;

  template<typename V>
  struct get_arithmetic_type<V, true> {
    using type = V;
  };

  template<typename V>
  struct get_arithmetic_type<V, false> {
    using type = typename V::underlying_type;
  };

  template<typename V, bool B>
  using get_arithmetic_type_t = typename get_arithmetic_type<V, B>::type;
public:
  using value_type = T;
  using size_type = size_t;
  using reference = value_type&;
  using const_reference = value_type const&;
  using pointer = value_type*;
  using const_pointer = value_type const*;
  using underlying_type = emb::circular_buffer<value_type, WindowSize>;
  using divider_type =
      get_arithmetic_type_t<value_type, std::is_arithmetic_v<value_type>>;
private:
  underlying_type data_;
  value_type sum_;
  value_type init_output_;
  value_type output_;
public:
  constexpr explicit movavg_filter(
      value_type const& init_output = value_type{})
    requires(WindowSize > 0)
      : init_output_(init_output) {
    reset();
  }

  constexpr explicit movavg_filter(
      size_type capacity,
      value_type const& init_output = value_type{})
    requires(WindowSize == 0)
      : data_(capacity), init_output_(init_output) {
    reset();
  }

  constexpr void push(value_type const& input_v) {
    if (!data_.full()) {
      data_.push_back(input_v);
      sum_ += input_v;
    } else {
      sum_ = sum_ - data_.front() + input_v;
      data_.push_back(input_v);
    }
    output_ = sum_ / static_cast<divider_type>(data_.size());
  }

  constexpr const_reference output() const { return output_; }

  constexpr void set_output(value_type const& output_v) {
    data_.clear();
    sum_ = value_type{0};
    output_ = output_v;
  }

  constexpr void reset() { set_output(init_output_); }

  constexpr underlying_type const& data() const { return data_; }

  constexpr size_type window_size() const { return data_.capacity(); }
};

} // namespace v2

namespace v1 {

#endif

template<typename T, size_t WindowSize>
class movavg_filter {
private:
  template<typename V>
  struct get_arithmetic_type;

  template<typename V, typename Unit>
  struct get_arithmetic_type<units::named_unit<V, Unit> > {
    typedef typename units::named_unit<V, Unit>::underlying_type type;
  };

  template<typename V>
  struct get_arithmetic_type {
    typedef V type;
  };
public:
  typedef T value_type;
  typedef size_t size_type;
  typedef value_type& reference;
  typedef value_type const& const_reference;
  typedef value_type* pointer;
  typedef value_type const* const_pointer;
#ifdef EMB_MOVING_AVERAGE_FILTER_V2
  typedef emb::v1::circular_buffer<value_type, WindowSize> underlying_type;
#else
  typedef emb::circular_buffer<value_type, WindowSize> underlying_type;
#endif
  typedef typename get_arithmetic_type<value_type>::type divider_type;
private:
  underlying_type data_;
  value_type sum_;
  value_type init_output_;
  value_type output_;
public:
  EMB_CONSTEXPR explicit movavg_filter(
      value_type const& init_output = T())
      : init_output_(init_output) {
    reset();
  }

  EMB_CONSTEXPR void push(value_type const& input_v) {
    if (!data_.full()) {
      data_.push_back(input_v);
      sum_ += input_v;
    } else {
      sum_ = sum_ - data_.front() + input_v;
      data_.push_back(input_v);
    }
    output_ = sum_ / static_cast<divider_type>(data_.size());
  }

  EMB_CONSTEXPR const_reference output() const { return output_; }

  EMB_CONSTEXPR void set_output(value_type const& output_v) {
    data_.clear();
    sum_ = value_type(0);
    output_ = output_v;
  }

  EMB_CONSTEXPR void reset() { set_output(init_output_); }

  EMB_CONSTEXPR underlying_type const& data() const { return data_; }

  EMB_CONSTEXPR size_type window_size() const { return data_.capacity(); }
};

#ifdef EMB_MOVING_AVERAGE_FILTER_V2

} // namespace v1

template<typename T>
struct is_moving_average_filter : std::false_type {};

template<typename T, size_t WindowSize>
struct is_moving_average_filter<movavg_filter<T, WindowSize>>
    : std::true_type {};

template<typename T, size_t WindowSize>
struct is_moving_average_filter<v1::movavg_filter<T, WindowSize>>
    : std::true_type {};

template<typename T>
concept MovingAverageFilter = is_moving_average_filter<T>::value;

#endif

} // namespace emb
