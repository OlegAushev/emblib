#pragma once

#include <emb/filter/exponential_filter.hpp>
#include <emb/filter/median_filter.hpp>

#include <cstddef>

namespace emb {

// The class template `median_exponential_filter` is a filter that connects a
// median of the last `WindowSize` values, which takes out a lone outlier if
// `WindowSize` is at least 3, to an exponential stage, which smooths what is
// left. The stages are a `median_filter` and an `exponential_filter`, and their
// constraints and contracts apply to it, e.g. an odd `WindowSize` and the rule
// on NaN of the median, and the smoothing factor and the time constant of the
// exponential stage. `set_time_constant` applies to that stage.
//
// The constructors are two ways to start, and which one to use depends on
// whether the value at the start is known. With that value as the initial
// output, the filter starts settled at it: both stages hold it, and the output
// keeps it for the next `WindowSize / 2` steps. Without an initial output, the
// filter waits for the window: the output stays at zero until the inputs have
// filled it and then starts at their median rather than climbing from zero, so
// a quantity already far from zero reads as itself after `WindowSize` values,
// not after several time constants. Passing zero as the initial output is not
// the same as passing none. `reset(value)` settles both stages on `value` in
// the same way, and ends the wait for the window, if any.
template<typename T, std::size_t WindowSize, typename Duration>
class median_exponential_filter {
public:
  using value_type = T;
  using duration_type = Duration;
  static constexpr std::size_t window_size = WindowSize;
private:
  emb::median_filter<value_type, window_size> median_;
  emb::exponential_filter<value_type, duration_type> smoother_;
public:
  constexpr median_exponential_filter(duration_type sampling_period,
                                      duration_type time_constant)
      : smoother_(sampling_period, time_constant)
  {
  }

  constexpr median_exponential_filter(duration_type sampling_period,
                                      duration_type time_constant,
                                      value_type init_output)
      : median_(init_output),
        smoother_(sampling_period, time_constant, init_output)
  {
  }

  constexpr void step(value_type input)
  {
    bool const started = median_.full();
    median_.push(input);
    if (started) {
      smoother_.step(median_.output());
    }
    else if (median_.full()) {
      smoother_.reset(median_.output());
    }
  }

  constexpr value_type output() const
  {
    return smoother_.output();
  }

  constexpr void reset(value_type value)
  {
    median_.reset(value);
    smoother_.reset(value);
  }

  constexpr void set_time_constant(duration_type value)
  {
    smoother_.set_time_constant(value);
  }

  friend constexpr void filter_feed(median_exponential_filter& f,
                                    value_type input)
  {
    f.step(input);
  }

  friend constexpr value_type filter_output(median_exponential_filter const& f)
  {
    return f.output();
  }
};

} // namespace emb
