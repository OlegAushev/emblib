#pragma once

#include <emb/math/saturation.hpp>

#include <cassert>
#include <cmath>
#include <concepts>
#include <limits>
#include <ratio>
#include <type_traits>

namespace emb {

// ---- saturate_round ----
template<std::integral Int, std::floating_point Float>
constexpr Int saturate_round(Float x)
{
  static_assert(sizeof(Int) < sizeof(long long) || std::is_signed_v<Int>,
                "u64 upper range is unreachable via llround");
  constexpr bool fits_long =
      sizeof(Int) < sizeof(long)
      || (sizeof(Int) == sizeof(long) && std::is_signed_v<Int>);
  using Wide = std::conditional_t<fits_long, long, long long>;

  assert(!std::isnan(x));
  if (x >= static_cast<Float>(std::numeric_limits<Wide>::max())) {
    return std::numeric_limits<Int>::max();
  }
  if (x <= static_cast<Float>(std::numeric_limits<Wide>::min())) {
    return std::numeric_limits<Int>::min();
  }

  if constexpr (fits_long) {
    return emb::saturating_cast<Int>(std::lround(x));
  }
  else {
    return emb::saturating_cast<Int>(std::llround(x));
  }
}

// ---- quantize ----
template<std::integral Int, typename Step, std::floating_point Float>
  requires requires {
    Step::num;
    Step::den;
  }
constexpr Int quantize(Float x)
{
  static_assert(Step::num > 0, "Step must be a positive ratio");
  constexpr Float scale =
      static_cast<Float>(Step::den) / static_cast<Float>(Step::num);
  return saturate_round<Int>(x * scale);
}

// ---- dequantize ----
template<typename Step, std::floating_point Float = float, std::integral Int>
  requires requires {
    Step::num;
    Step::den;
  }
constexpr Float dequantize(Int n)
{
  static_assert(Step::num > 0, "Step must be a positive ratio");
  constexpr Float step =
      static_cast<Float>(Step::num) / static_cast<Float>(Step::den);
  return static_cast<Float>(n) * step;
}

// --------------------------------------------------------------------- scaled

// A wrapper that names what a floating-point scalar means.
// Types `emb::units::named_unit` and `emb::clamped` both satisfy it.
template<typename T>
concept some_quantity = requires(T q, typename T::value_type v) {
  requires std::floating_point<typename T::value_type>;
  { T(v) } -> std::same_as<T>;
  { q.value() } -> std::same_as<typename T::value_type>;
};

// Anything that reduces to a `std::ratio`. Alias `::type` is the reduced
// spelling, which is what the alias below hands to the template.
template<typename R>
concept some_ratio = requires {
  typename R::type;
  R::num;
  R::den;
} && std::same_as<typename R::type, std::ratio<R::num, R::den>>;

namespace detail {

// An integer as it sits in a frame, a record or a register, bound to the
// quantity it stands for. Step is the physical value of one count:
// a pu field with a step of 1/10000 reads +-1.0 over +-10000 counts.
template<std::integral Raw, some_quantity Q, some_ratio Step>
class scaled {
public:
  using raw_type = Raw;
  using quantity_type = Q;
  using step_type = Step;
  using value_type = typename Q::value_type;
private:
  static_assert(step_type::num > 0, "Step must be a positive ratio");

  raw_type raw_{};
public:
  scaled() = default;

  constexpr explicit scaled(quantity_type q) : raw_{encode(q)} {}

  constexpr scaled& operator=(quantity_type q)
  {
    raw_ = encode(q);
    return *this;
  }

  constexpr quantity_type value() const
  {
    return quantity_type(dequantize<step_type, value_type>(raw_));
  }

  constexpr raw_type raw() const
  {
    return raw_;
  }
private:
  static constexpr raw_type encode(quantity_type q)
  {
    return quantize<raw_type, step_type>(q.value());
  }
};

} // namespace detail

// The public form of `detail::scaled`. Step is reduced here because equal
// ratios need not be the same type.
template<std::integral Raw, some_quantity Q, some_ratio Step>
using scaled = detail::scaled<Raw, Q, typename Step::type>;

} // namespace emb
