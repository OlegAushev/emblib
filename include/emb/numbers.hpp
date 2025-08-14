#pragma once

#if __cplusplus >= 201907
#include <numbers>
#endif

namespace emb {

namespace numbers {
#ifdef __cpp_lib_math_constants
inline constexpr float pi = std::numbers::pi_v<float>;
inline constexpr float pi_over_2 = pi / 2.0f;
inline constexpr float pi_over_4 = pi / 4.0f;
inline constexpr float pi_over_3 = pi / 3.0f;
inline constexpr float pi_over_6 = pi / 6.0f;
inline constexpr float two_pi = 2.0f * pi;
inline constexpr float sqrt_2 = std::numbers::sqrt2_v<float>;
inline constexpr float sqrt_3 = std::numbers::sqrt3_v<float>;
inline constexpr float inv_sqrt3 = std::numbers::inv_sqrt3_v<float>;
#else
const float pi = 3.1415926535897932384626433832795f;
float const pi_over_2 = 1.570796326794897f;
float const pi_over_4 = 0.785398163397448f;
float const pi_over_3 = 1.047197551196598f;
float const pi_over_6 = 0.5235987755982988f;
float const two_pi = 6.283185307179586f;
float const sqrt_2 = 1.414213562373095048801688724209698079f;
float const sqrt_3 = 1.732050807568877293527446341505872367f;
float const inv_sqrt3 = 0.577350269189625764509148780501957456f;
#endif
} // namespace numbers

}
