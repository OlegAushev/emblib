#pragma once

#include <emb/units/named_unit.hpp>

#include <concepts>

namespace emb {
namespace units {

namespace tags {

struct amp {};
struct volt {};

} // namespace tags

template<std::floating_point T>
using amp = named_unit<T, tags::amp>;

template<std::floating_point T>
using volt = named_unit<T, tags::volt>;

using amp_f32 = amp<float>;
using volt_f32 = volt<float>;

} // namespace units
} // namespace emb
