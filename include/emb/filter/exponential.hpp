#pragma once

#include <algorithm>
#include <utility>

namespace emb {
namespace filter {

template<typename T, typename Duration>
class exponential {
public:
  using value_type = T;
  using reference = value_type&;
  using const_reference = value_type const&;
  using duration_type = Duration;
  using factor_type =
      decltype(std::declval<Duration>() / std::declval<Duration>());
private:
  duration_type sampling_period_;
  duration_type time_constant_;
  factor_type smooth_factor_;
  value_type init_output_;
  value_type output_;
public:
  constexpr exponential(
      duration_type sampling_period,
      duration_type time_constant,
      value_type const& init_output = value_type()
  )
      : init_output_(init_output) {
    configure(sampling_period, time_constant);
    reset();
  }

  constexpr void push(value_type const& input_v) {
    output_ = output_ + smooth_factor_ * (input_v - output_);
  }

  constexpr const_reference output() const {
    return output_;
  }

  constexpr void set_output(value_type const& output_v) {
    output_ = output_v;
  }

  constexpr void reset() {
    set_output(init_output_);
  }

  constexpr void configure(duration_type sampling_period, duration_type time_constant) {
    sampling_period_ = sampling_period;
    time_constant_ = time_constant;
    smooth_factor_ = std::clamp(
        sampling_period / time_constant,
        factor_type(0),
        factor_type(1)
    );
  }

  constexpr void set_sampling_period(duration_type ts) {
    sampling_period_ = ts;
    smooth_factor_ = std::clamp(
        sampling_period_ / time_constant_,
        factor_type(0),
        factor_type(1)
    );
  }

  constexpr factor_type smooth_factor() const {
    return smooth_factor_;
  }
};

template<typename T>
struct is_exponential_filter_type : std::false_type {};

template<typename T, typename Duration>
struct is_exponential_filter_type<exponential<T, Duration>> : std::true_type {};

template<typename T>
concept exponential_filter_type = is_exponential_filter_type<T>::value;

} // namespace filter
} // namespace emb
