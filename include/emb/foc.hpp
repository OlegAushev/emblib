#pragma once

#include <emb/foc/clarke.hpp>
#include <emb/foc/deadtime_compensation.hpp>
#include <emb/foc/dq_controller.hpp>
#include <emb/foc/park.hpp>
#include <emb/foc/sinpwm.hpp>
#include <emb/foc/svpwm.hpp>
#include <emb/foc/to_polar.hpp>
#include <emb/foc/types.hpp>
#include <emb/foc/utility.hpp>
#include <emb/pipe.hpp>

namespace emb {
namespace foc {
using emb::pipe::operator|;
using emb::pipe::tap;
using emb::pipe::store_to;
using emb::pipe::transform;
} // namespace foc
} // namespace emb
