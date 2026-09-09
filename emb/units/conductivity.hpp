#pragma once

#include <emb/units/named_unit.hpp>

#include <concepts>

namespace emb {
namespace units {

namespace tags {

struct microsiemens_per_cm {};

} // namespace tags

template<std::floating_point T>
using microsiemens_per_cm = named_unit<T, tags::microsiemens_per_cm>;

using microsiemens_per_cm_f32 = microsiemens_per_cm<float>;

} // namespace units
} // namespace emb
