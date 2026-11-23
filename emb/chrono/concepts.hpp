#pragma once

#include <chrono>
#include <type_traits>

namespace emb::chrono {

template<typename T>
struct is_duration : std::false_type {};

template<typename Rep, typename Period>
struct is_duration<std::chrono::duration<Rep, Period>> : std::true_type {};

template<typename T>
inline constexpr bool is_duration_v = is_duration<T>::value;

template<typename T>
concept some_duration = is_duration_v<T>;

}