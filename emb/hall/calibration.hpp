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

// A calibrated angle lies in [`min_calibration_angle`,
// `max_calibration_angle`).
inline constexpr emb::units::edeg_f32 min_calibration_angle{0.0f};
inline constexpr emb::units::edeg_f32 max_calibration_angle{360.0f};

// Checks that `data` describes a usable geometry: every angle lies in
// [`min_calibration_angle`, `max_calibration_angle`), and the six angles of
// each direction differ. Returns `error::invalid_calibration` if not. The
// order of the sectors is not checked.
constexpr std::expected<void, error> validate(calibration_result const& data)
{
  auto const usable = [](sector_map<emb::units::edeg_f32> const& angles) {
    auto const& a = angles.data();
    for (auto i = 0uz; i < a.size(); ++i) {
      // written so that NaN fails it too
      if (!(a[i] >= min_calibration_angle && a[i] < max_calibration_angle)) {
        return false;
      }
      for (auto j = 0uz; j < i; ++j) {
        if (a[j] == a[i]) {
          return false;
        }
      }
    }
    return true;
  };

  if (!usable(data.fwd_sector_angles) || !usable(data.rev_sector_angles)) {
    return std::unexpected(error::invalid_calibration);
  }
  return {};
}

struct sector_geometry {
  sector_map<sector_span> fwd;
  sector_map<sector_span> rev;
  transition_sign_map signs;
};

sector_geometry make_geometry(calibration_result const& data);

} // namespace emb::hall
