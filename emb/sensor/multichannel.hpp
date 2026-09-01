#pragma once

#include <emb/meta.hpp>
#include <emb/sensor/concepts.hpp>

#include <array>
#include <concepts>
#include <cstddef>
#include <tuple>
#include <type_traits>
#include <utility>

namespace emb::sensor {

// N-channel sensor core: an aggregate of N independent singlechannel cores
// driven by one acquisition frame. Adds only frame-synchronous submission, so
// the N filtered outputs read back from value()/values() belong to the same
// acquisition frame.
//
// The channels may be different core types -- inputs with different
// transducer models behind one ADC scan -- but they share the frame, so they
// share sample_type and raw_type, and they make up one sensor, so they share
// value_type: the frame reads back as one std::array whatever the channels
// are. Identical channel types (multichannel_n) live in an array and are
// addressed by index directly, with core(channel) available; otherwise
// channels live in a tuple, reached through core<I>(), and value(channel)
// selects through a compare chain.
template<typename... Cores>
  requires(sizeof...(Cores) > 0)
       && (some_singlechannel_sensor<Cores> && ...)
       && (some_immediate_sensor<Cores> && ...)
       && all_same<typename Cores::raw_type...>
       && all_same<typename Cores::sample_type...>
       && all_same<typename Cores::value_type...>
class multichannel {
  using first_type = nth_type_t<0, Cores...>;
public:
  static constexpr std::size_t channel_count = sizeof...(Cores);
  static constexpr bool uniform_cores = all_same<Cores...>;

  template<std::size_t I>
  using core_type = nth_type_t<I, Cores...>;

  using raw_type = typename first_type::raw_type;
  using sample_type =
      std::array<typename first_type::sample_type, channel_count>;
  using value_type = typename first_type::value_type;
  using values_type = std::array<value_type, channel_count>;
  using sensor_category = immediate_tag;
  using cores_type = std::conditional_t<uniform_cores,
                                        std::array<first_type, channel_count>,
                                        std::tuple<Cores...>>;
private:
  cores_type cores_;

  template<typename... Args, std::size_t... I>
  constexpr multichannel(std::index_sequence<I...>, Args const&... args)
      : cores_{((void)I, first_type(args...))...}
  {
  }
public:
  constexpr multichannel() = default;

  // Per-channel construction: each channel gets its own core, e.g. with
  // independent per-channel gain/offset calibration.
  constexpr explicit multichannel(cores_type cores)
    requires(std::move_constructible<Cores> && ...)
      : cores_(std::move(cores))
  {
  }

  // Broadcast construction: every channel's core is built from the same
  // arguments. Each channel still keeps its own independent state. Uniform
  // channels only: cores of different types differ in what they are built
  // from.
  template<typename... Args>
    requires uniform_cores
          && (sizeof...(Args) > 0)
          && std::constructible_from<first_type, Args const&...>
  constexpr explicit multichannel(Args const&... args)
      : multichannel(std::make_index_sequence<channel_count>{}, args...)
  {
  }

  template<std::size_t I>
  constexpr core_type<I> const& core() const
  {
    return std::get<I>(cores_);
  }

  template<std::size_t I>
  constexpr core_type<I>& core()
  {
    return std::get<I>(cores_);
  }

  constexpr first_type const& core(std::size_t channel) const
    requires uniform_cores
  {
    return cores_[channel];
  }

  constexpr first_type& core(std::size_t channel)
    requires uniform_cores
  {
    return cores_[channel];
  }

  constexpr void submit(sample_type const& sample)
  {
    unroll<channel_count>(
        [&]<std::size_t I>() { std::get<I>(cores_).submit(sample[I]); });
  }

  // `channel` must be below channel_count.
  constexpr value_type value(std::size_t channel) const
  {
    return visit_at(cores_, channel, [](auto const& core) {
      return core.value();
    });
  }

  constexpr values_type values() const
  {
    return [&]<std::size_t... I>(std::index_sequence<I...>) {
      return values_type{std::get<I>(cores_).value()...};
    }(std::make_index_sequence<channel_count>{});
  }

  template<typename V>
  constexpr V values() const
  {
    return [&]<std::size_t... I>(std::index_sequence<I...>) {
      return V{std::get<I>(cores_).value()...};
    }(std::make_index_sequence<channel_count>{});
  }
};

// multichannel<Core, Core, ...>: N channels of one core type, for a channel
// count known as a constant rather than a spelled-out list.
template<typename Core, std::size_t N>
  requires(N > 0)
using multichannel_n = replicate_t<multichannel, Core, N>;

} // namespace emb::sensor
