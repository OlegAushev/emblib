#pragma once

#include <emb/math/saturation.hpp>

#include <cassert>
#include <cmath>
#include <concepts>
#include <limits>
#include <ratio>
#include <type_traits>

namespace emb {

// Rounds `x` to the nearest integer, rounding halfway cases away from zero,
// and clamps the result to the range of `Int`. Positive infinity gives
// `std::numeric_limits<Int>::max()` and negative infinity gives
// `std::numeric_limits<Int>::min()`. If `x` is NaN, an `assert` fails; if
// `NDEBUG` is defined, the result is unspecified.
//
// The program is ill-formed if `Int` is not a signed or unsigned integer
// type, or if it is an unsigned type at least as wide as `long long`, such as
// `std::uint64_t`.
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

// Converts the physical value `x` to a count of steps, where `Step` is a
// ratio type, such as `std::ratio`, giving the physical value of one step.
// E.g. with a `Step` of `std::ratio<1, 10000>`, 1.0 gives 10000 and -1.0
// gives -10000.
//
// Multiplies `x` by the scale factor `Step::den` / `Step::num`, computed in
// `Float` at compile time, and converts the product to `Int` as if by
// `saturate_round<Int>`, i.e. rounds it half away from zero and saturates it
// to the range of `Int`. The scale factor is exact if `Step::num` is a power
// of two and `Step::den` is exactly representable in `Float`, e.g. for
// `std::ratio<1, 10000>`, and rounded otherwise. The product is rounded to
// `Float` before it is rounded to an integer, so where the exact quotient of
// `x` and the step lies close to a halfway point between two integers, the
// result can differ by one count from that quotient rounded and saturated
// the same way. If `Step::num` and `Step::den` are exactly representable in
// `Float`, the difference never exceeds one count while the magnitude of the
// quotient is below 2^23 for `float` (2^52 for `double`).
//
// `Step` can be any class for which `Step::num` and `Step::den` are constant
// expressions. The program is ill-formed if `Step::num` or `Step::den` is not
// positive.
template<std::integral Int, typename Step, std::floating_point Float>
  requires requires {
    Step::num;
    Step::den;
  }
constexpr Int quantize(Float x)
{
  static_assert(Step::num > 0 && Step::den > 0,
                "Step must be a positive ratio");
  constexpr Float scale =
      static_cast<Float>(Step::den) / static_cast<Float>(Step::num);
  return saturate_round<Int>(x * scale);
}

// Converts the count of steps `n` to the physical value it stands for, where
// `Step` is a ratio type, such as `std::ratio`, giving the physical value of
// one step. E.g. with a `Step` of `std::ratio<1, 10000>`, 10000 gives 1.0 and
// -10000 gives -1.0.
//
// Converts `n` to `Float` and multiplies it by the step
// `Step::num` / `Step::den`, computed in `Float` at compile time. The step is
// exact if `Step::den` is a power of two and `Step::num` is exactly
// representable in `Float`, e.g. for `std::ratio<1, 2>`, and rounded
// otherwise. The conversion of `n` is exact while its magnitude is at most
// 2^24 for `float` (2^53 for `double`); beyond that, a count that `Float`
// cannot represent is rounded, so distinct counts can give the same result.
// The product is rounded to `Float`. If `Step::num` and `Step::den` are
// exactly representable in `Float` and the conversion of `n` is exact, the
// relative error of the result is below 2^-23 for `float` (2^-52 for
// `double`). Even where the step is rounded, `quantize<Int, Step, Float>`
// gives back `n` from the result while the magnitude of `n` is below 2^21 for
// `float` (2^50 for `double`).
//
// `Step` can be any class for which `Step::num` and `Step::den` are constant
// expressions. The program is ill-formed if `Step::num` or `Step::den` is not
// positive.
template<typename Step, std::floating_point Float = float, std::integral Int>
  requires requires {
    Step::num;
    Step::den;
  }
constexpr Float dequantize(Int n)
{
  static_assert(Step::num > 0 && Step::den > 0,
                "Step must be a positive ratio");
  constexpr Float step =
      static_cast<Float>(Step::num) / static_cast<Float>(Step::den);
  return static_cast<Float>(n) * step;
}

// The concept `some_quantity<T>` specifies that `T` is a class that wraps a
// physical value of floating-point type. It is satisfied if and only if
// `T::value_type` is a floating-point type, `T(v)` constructs a `T` from a `v`
// of that type, and `q.value()` returns a `T::value_type`, not a reference to
// one, for a `q` of type `T`. It is modeled only if `T(q.value())` wraps the
// same physical value as `q`.
//
// `units::named_unit` satisfies it, and so does `clamped` with floating-point
// bounds, e.g. `signed_pu_f32`; arithmetic types such as `float` do not.
template<typename T>
concept some_quantity = requires(T q, typename T::value_type v) {
  requires std::floating_point<typename T::value_type>;
  { T(v) } -> std::same_as<T>;
  { q.value() } -> std::same_as<typename T::value_type>;
};

// The concept `some_ratio<R>` is satisfied if and only if `R::type` denotes
// the same type as `std::ratio<R::num, R::den>`. Every specialization of
// `std::ratio` satisfies it. The ratio can be zero or negative.
//
// For a specialization of `std::ratio`, `R::type` is the same ratio in lowest
// terms, e.g. `std::ratio<3, 30>::type` is `std::ratio<1, 10>`, so ratios of
// equal value have the same `R::type` even if they are distinct types.
template<typename R>
concept some_ratio = requires {
  typename R::type;
  R::num;
  R::den;
} && std::same_as<typename R::type, std::ratio<R::num, R::den>>;

namespace detail {

// The class template `scaled` stores a quantity of type `Q` as a count of
// steps of type `Raw`, the raw value, where `Step` is a ratio type, such as
// `std::ratio`, giving the physical value of one step. E.g. with a `Step` of
// `std::ratio<1, 10000>`, a physical value of 1.0 is stored as 10000.
// `scaled` is trivially copyable and has the size and alignment of `Raw`, and
// its object representation is that of the raw value, so it can stand in for
// a `Raw` member of a structure that is converted to and from bytes, e.g. by
// `std::bit_cast`.
//
// A default-constructed `scaled` has a raw value of zero. Constructing a
// `scaled` from a quantity, or assigning a quantity to it, converts the
// physical value to a count of steps as if by `quantize<Raw, Step>`, rounding
// half away from zero and saturating to the range of `Raw`. If the physical
// value is NaN, an `assert` fails; if `NDEBUG` is defined, the raw value is
// unspecified. `value()` converts the raw value back as if by
// `dequantize<Step, value_type>` and returns a `Q` constructed from the
// result. Unless constructing a `Q` changes the physical value, e.g. by
// clamping it, constructing a `scaled` from `value()` gives back the raw value
// while the magnitude of the raw value is below 2^21 for a `value_type` of
// `float` (2^50 for `double`).
//
// The program is ill-formed if `Step` is not positive. A program that
// converts a quantity to a raw value is ill-formed if `Raw` is not a signed
// or unsigned integer type, or if it is an unsigned type at least as wide as
// `long long`, such as `std::uint64_t`.
template<std::integral Raw, some_quantity Q, some_ratio Step>
class scaled {
public:
  using raw_type = Raw;
  using quantity_type = Q;
  using step_type = Step;
  using value_type = typename Q::value_type;
private:
  static_assert(step_type::num > 0 && step_type::den > 0,
                "Step must be a positive ratio");

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

// `scaled` is an alias template for the class template `detail::scaled`, with
// `Step::type` in place of `Step`. For a specialization of `std::ratio` or a
// class derived from one, `Step::type` is the same ratio in lowest terms, so
// ratios of equal value give the same type even if they are distinct types,
// e.g. `std::ratio<3, 30>` and `std::ratio<1, 10>`. For any other `Step`, this
// holds only if `Step::type` is in lowest terms.
template<std::integral Raw, some_quantity Q, some_ratio Step>
using scaled = detail::scaled<Raw, Q, typename Step::type>;

} // namespace emb
