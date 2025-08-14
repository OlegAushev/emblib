#pragma once

#if __cplusplus >= 201907
#include <numbers>
#endif

namespace emb {

namespace numbers {
#ifdef __cpp_lib_math_constants
inline constexpr float pi = std::numbers::pi_v<float>;
inline constexpr float sqrt2 = std::numbers::sqrt2_v<float>;
inline constexpr float sqrt3 = std::numbers::sqrt3_v<float>;
inline constexpr float inv_sqrt3 = std::numbers::inv_sqrt3_v<float>;
#else
const float pi = 3.1415926535897932384626433832795f;
float const sqrt2 = 1.414213562373095048801688724209698079f;
float const sqrt3 = 1.732050807568877293527446341505872367f;
float const inv_sqrt3 = 0.577350269189625764509148780501957456f;
#endif
} // namespace numbers

}
