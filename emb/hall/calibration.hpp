#pragma once

#include <emb/hall/error.hpp>
#include <emb/hall/sector.hpp>
#include <emb/units.hpp>

#include <expected>

namespace emb::hall {

struct calibration_result {
  sector_map<emb::units::edeg_f32> fwd_sector_angles;
  sector_map<emb::units::edeg_f32> rev_sector_angles;
};

constexpr std::expected<void, error> validate(calibration_result const&)
{
  // TODO
  return {};
}

struct sector_geometry {
  sector_map<sector_span> fwd;
  sector_map<sector_span> rev;
  transition_sign_map signs;
};

sector_geometry make_geometry(calibration_result const& data);

} // namespace emb::hall
