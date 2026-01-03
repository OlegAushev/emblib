#pragma once

#include <emb/algorithm.hpp>
#include <emb/core.hpp>
#include <emb/units.hpp>

#include <algorithm>

namespace emb {

template<typename T>
class ramp_generator {
private:
  units::sec_f32 update_period_;
  T slope_;
  T step_;

  T target_;
  T init_output_;
  T output_;
public:
  ramp_generator(
      units::sec_f32 const& update_period,
      T const& slope,
      T const& init_output = T()
  )
      : init_output_(init_output) {
    configure(update_period, slope);
    reset();
  }

  T target() const {
    return target_;
  }

  T output() const {
    return output_;
  }

  void set_target(T const& input_v) {
    target_ = input_v;
  }

  void set_output(T const& output_v) {
    target_ = output_v;
    output_ = output_v;
  }

  void reset() {
    set_output(init_output_);
  }

  void configure(units::sec_f32 const& update_period, T const& slope) {
    assert(update_period.value() > 0);
    assert(slope > T(0));
    update_period_ = update_period;
    slope_ = slope;
    step_ = update_period.value() * slope;
  }

  void update() {
    if (output_ < target_) {
      output_ = std::min(output_ + step_, target_);
    } else {
      output_ = std::max(output_ - step_, target_);
    }
  }

  bool steady() const {
    return output_ == target_;
  }
};

} // namespace emb
