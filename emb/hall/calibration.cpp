#include <emb/hall/calibration.hpp>

namespace emb::hall {

namespace {

sector_map<sector_span>
make_spans(sector_map<emb::units::edeg_f32> const& angles, direction dir)
{
  auto const to_rad = [](emb::units::edeg_f32 v) {
    return emb::norm2pi(emb::units::convert_to<emb::units::erad_f32>(v));
  };

  auto const widths = compute_sector_widths(angles, dir);

  sector_map<sector_span> spans;
  for (auto i = 0uz; i < 6; ++i) {
    sector const s = static_cast<sector>(i);
    spans[s] = {to_rad(angles[s]), to_rad(widths[s])};
  }
  return spans;
}

} // namespace

sector_geometry make_geometry(calibration_result const& data)
{
  return {.fwd = make_spans(data.fwd_sector_angles, direction::fwd),
          .rev = make_spans(data.rev_sector_angles, direction::rev),
          .signs = transition_sign_map(data.fwd_sector_angles)};
}

} // namespace emb::hall
