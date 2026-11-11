#pragma once

#include <emb/sensor/concepts.hpp>

#include <concepts>
#include <cstdint>
#include <expected>
#include <optional>
#include <type_traits>
#include <utility>

namespace emb::sensor {

// The enumeration `fault` names why a `validated` sensor has no output to
// give: `no_sample` before its first sample, and `below` or `above` when its
// last sample lies below or above the raw values that its measurement chain
// gives while healthy, e.g. the value of a shorted or of an open thermistor. A
// validator finds `below` or `above`; `no_sample` is the state of the sensor
// alone.
enum class fault : std::uint8_t {
  no_sample,
  below,
  above
};

// The class template `validated` is a sensor that judges each sample with the
// validator `Validator` before the sample reaches `Core`, an immediate
// single-channel sensor such as a `singlechannel`. `update(sample)` calls the
// validator on `sample` and passes `sample` on to the `update` of `Core` if
// the verdict is empty; a sample that the validator refuses does not reach
// `Core`, so the conversion and the filter of `Core` run on accepted samples
// alone. `output()` returns `std::expected<typename Core::output_type, fault>`:
// the output of `Core` if the last sample was accepted, the fault that the
// validator found in it otherwise, and `fault::no_sample` before the first
// sample, even if the filter of `Core` starts at an initial output, which
// `core().output()` still returns.
//
// `Validator` is called as const with a const lvalue of `input_type` and
// returns `std::optional<fault>`, the verdict on the sample: empty if the
// measurement chain gives such a sample while healthy, and otherwise the
// fault that the sample shows. The call is const, so a validator judges each
// sample by itself, and its result is `std::optional<fault>` itself, not a
// type that converts to one: a validator returning a bare `fault` would find
// a fault in every sample, and does not compile.
//
// To the filter of `Core`, a refused sample is one that was never taken: the
// filter keeps its output over a run of refused samples, and an
// `exponential_filter`, whose smoothing factor assumes its sampling period,
// moves on from that output by one step per accepted sample. The verdict is
// that of the last sample alone, and `validated` has no notion of time: a
// policy over time, such as a run of faults long enough to report or a fault
// that stays reported, belongs to the caller.
//
// `output_type` has no order, unlike `std::optional`, whose comparison with a
// value is false or true when it holds none: a check of the output against a
// threshold compiles only once it takes the value out, so no such check
// passes over a missing value unnoticed. The value is read with `*` or `->`
// once `has_value()` holds; `value()` throws `std::bad_expected_access` on a
// fault, which without exceptions ends the program.
//
// A `validated` is itself an immediate single-channel sensor, so it serves as
// a core of `multichannel` or `buffered` and as a source of `multiplexed`,
// each channel or source with a verdict of its own. It wraps the sensor that
// converts directly: a `buffered` is a deferred sensor, around which the
// verdict would fall when a sample is parked rather than when it is
// converted, and neither `multichannel` nor `multiplexed` is a single-channel
// sensor, so none of them is a `Core`.
//
// The parts are reachable as those of `singlechannel` are, under its rule:
// `validator()` and `core()`, and `converter()` and `filter()` if `Core` has
// them, so code that calibrates or retunes a sensor reaches its stages
// through `validated` as it would through `Core`. Mutation through them
// belongs to the context that runs `update(sample)`, or to a quiescent
// sensor.
template<typename Validator, typename Core>
  requires some_immediate_sensor<Core>
        && some_singlechannel_sensor<Core>
        && std::invocable<Validator const&, typename Core::input_type const&>
        && std::same_as<std::invoke_result_t<Validator const&,
                                             typename Core::input_type const&>,
                        std::optional<fault>>
class validated {
public:
  using core_type = Core;
  using input_type = typename Core::input_type;
  using output_type = std::expected<typename Core::output_type, fault>;
  using sensor_category = immediate_tag;
private:
  [[no_unique_address]] Validator validator_{};
  Core core_;
  std::optional<fault> fault_{fault::no_sample};
public:
  constexpr validated() = default;

  // Constructs the core from `args` and value-initializes the validator, so
  // that `multichannel` and `multiplexed` build every channel or source from
  // the arguments of its core, as `buffered` builds its core.
  template<typename... Args>
    requires(sizeof...(Args) > 0)
         && std::default_initializable<Validator>
         && std::constructible_from<Core, Args...>
  constexpr explicit validated(Args&&... args)
      : core_(std::forward<Args>(args)...)
  {
  }

  // Takes the validator, e.g. one that holds its range, and the core by
  // value, as `singlechannel` takes its stages.
  constexpr validated(Validator validator, Core core)
      : validator_(std::move(validator)), core_(std::move(core))
  {
  }

  constexpr Validator const& validator() const
  {
    return validator_;
  }

  constexpr Validator& validator()
  {
    return validator_;
  }

  constexpr Core const& core() const
  {
    return core_;
  }

  constexpr Core& core()
  {
    return core_;
  }

  constexpr decltype(auto) converter() const
    requires requires(Core const& c) { c.converter(); }
  {
    return core_.converter();
  }

  constexpr decltype(auto) converter()
    requires requires(Core& c) { c.converter(); }
  {
    return core_.converter();
  }

  constexpr decltype(auto) filter() const
    requires requires(Core const& c) { c.filter(); }
  {
    return core_.filter();
  }

  constexpr decltype(auto) filter()
    requires requires(Core& c) { c.filter(); }
  {
    return core_.filter();
  }

  constexpr output_type output() const
  {
    if (fault_) {
      return std::unexpected(*fault_);
    }
    return core_.output();
  }

  constexpr void update(input_type sample)
  {
    fault_ = validator_(sample);
    if (!fault_) {
      core_.update(std::move(sample));
    }
  }
};

} // namespace emb::sensor
