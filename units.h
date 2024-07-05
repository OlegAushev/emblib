#pragma once


#include <emblib/core.h>


namespace emb {
namespace units {


namespace impl {
struct rad_t{};
struct deg_t{};
struct elec_rad_t{};
struct mech_rad_t{};
struct elec_deg_t{};
struct mech_deg_t{};
struct radps_t{};
struct rpm_t{};
}


const impl::rad_t rad;
const impl::deg_t deg;
const impl::elec_rad_t elec_rad;
const impl::mech_rad_t mech_rad;
const impl::elec_deg_t elec_deg;
const impl::mech_deg_t mech_deg;
const impl::radps_t radps;
const impl::rpm_t rpm;


} // namespace units
} // namespace emb
