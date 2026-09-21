#pragma once

#include <emb/gpio.hpp>
#include <emb/hall/angle_sensor.hpp>
#include <emb/hall/sector.hpp>
#include <emb/units.hpp>

#include <algorithm>
#include <array>
#include <utility>

namespace emb::hall {

namespace detail {

// An ideal set at the nominal placement, switching late by 40 electrical
// degrees in whichever direction it travels. The lag is deliberate: a
// sensor that switched at the same angle both ways would let a consumer
// get its calibration wrong and never notice.
inline constexpr sector_map<emb::units::edeg_f32> emu_fwd_entries =
    sector_map<emb::units::edeg_f32>{{emb::units::edeg_f32{330},
                                      emb::units::edeg_f32{30},
                                      emb::units::edeg_f32{90},
                                      emb::units::edeg_f32{150},
                                      emb::units::edeg_f32{210},
                                      emb::units::edeg_f32{270}}};

inline constexpr sector_map<emb::units::edeg_f32> emu_rev_entries =
    sector_map<emb::units::edeg_f32>{{emb::units::edeg_f32{-10},
                                      emb::units::edeg_f32{50},
                                      emb::units::edeg_f32{110},
                                      emb::units::edeg_f32{170},
                                      emb::units::edeg_f32{230},
                                      emb::units::edeg_f32{290}}};

struct sorted_entries {
  std::array<emb::units::erad_f32, 6> angles;
  std::array<sector, 6> sectors;
};

constexpr sorted_entries
to_sorted_entries(sector_map<emb::units::edeg_f32> const& entries)
{
  using pair = std::pair<emb::units::erad_f32, sector>;
  std::array<pair, 6> tmp;
  for (auto i = 0uz; i < 6; ++i) {
    auto s = static_cast<sector>(i);
    tmp[i] = {
        emb::norm2pi(emb::units::convert_to<emb::units::erad_f32>(entries[s])),
        s};
  }
  std::sort(tmp.begin(), tmp.end());

  sorted_entries result{};
  for (auto i = 0uz; i < 6; ++i) {
    result.angles[i] = tmp[i].first;
    result.sectors[i] = tmp[i].second;
  }
  return result;
}

inline constexpr auto emu_fwd = to_sorted_entries(emu_fwd_entries);
inline constexpr auto emu_rev = to_sorted_entries(emu_rev_entries);

constexpr sector lookup_fwd(emb::units::erad_f32 angle)
{
  sector s = emu_fwd.sectors[5];
  for (auto i = 6uz; i-- > 0;) {
    if (angle >= emu_fwd.angles[i]) {
      s = emu_fwd.sectors[i];
      break;
    }
  }
  return s;
}

constexpr sector lookup_rev(emb::units::erad_f32 angle)
{
  sector s = emu_rev.sectors[0];
  for (auto i = 0uz; i < 6; ++i) {
    if (angle <= emu_rev.angles[i]) {
      s = emu_rev.sectors[i];
      break;
    }
  }
  return s;
}

// What an ideal hall set reads at this angle, given what it last read.
// The forward and reverse tables disagree over the span between them --
// that span is the hysteresis, and inside it the previous answer stands.
//
// precondition: angle is in [0, 2pi)
constexpr sector sector_at(emb::units::erad_f32 angle, sector current)
{
  auto const fwd_s = lookup_fwd(angle);
  auto const rev_s = lookup_rev(angle);
  return (fwd_s == rev_s) ? fwd_s : current;
}

constexpr emb::units::erad_f32 deg(float d)
{
  return emb::norm2pi(
      emb::units::convert_to<emb::units::erad_f32>(emb::units::edeg_f32{d}));
}

// well inside a sector: both fwd and rev agree, any current sector works
static_assert(sector_at(deg(340), sector::ca) == sector::a);
static_assert(sector_at(deg(40), sector::a) == sector::ab);
static_assert(sector_at(deg(100), sector::ab) == sector::b);
static_assert(sector_at(deg(160), sector::b) == sector::bc);
static_assert(sector_at(deg(220), sector::bc) == sector::c);
static_assert(sector_at(deg(280), sector::c) == sector::ca);

// hysteresis zone between ca and a:
//   rev boundary for ca at 290 deg, fwd boundary for a at 330 deg
// before both boundaries: both say ca
static_assert(sector_at(deg(280), sector::ca) == sector::ca);
// in hysteresis zone (290..330): fwd=ca, rev=a -> disagree -> keep current
static_assert(sector_at(deg(310), sector::ca) == sector::ca);
static_assert(sector_at(deg(310), sector::a) == sector::a);
// past both boundaries (>=330): both say a
static_assert(sector_at(deg(340), sector::ca) == sector::a);
static_assert(sector_at(deg(340), sector::a) == sector::a);

} // namespace detail

// detail::sector_at, out of line: an emulator driven from an interrupt
// calls it from every branch of a mode switch, and two table walks inlined
// six times cost more than the call.
sector emulated_sector(emb::units::erad_f32 angle, sector current);

// Drives three pins the way a hall set on a turning rotor would, so a
// drive can be exercised with no machine attached. It integrates the angle
// it is given a speed for, or simply follows the angle it is handed.
template<emb::gpio::output Pin>
class emulator {
public:
  using outputs = std::array<Pin, 3>;
private:
  outputs outputs_;
  emb::units::sec_f32 update_period_;

  emb::units::erad_f32 angle_{0};
  sector sector_{sector::a};
public:
  template<typename Config>
  emulator(std::array<Config, 3> const& outputs_conf,
           some_timebase auto const& timebase)
      : outputs_{Pin(outputs_conf[0]),
                 Pin(outputs_conf[1]),
                 Pin(outputs_conf[2])},
        update_period_(timebase.period())
  {
  }

  void update(emb::units::eradps_f32 speed)
  {
    angle_ = emb::norm2pi_fast(angle_ + speed * update_period_);
    sector_ = emulated_sector(angle_, sector_);
    apply_outputs(sector_);
  }

  void update(emb::units::erad_f32 angle)
  {
    angle_ = emb::norm2pi_fast(angle);
    sector_ = emulated_sector(angle_, sector_);
    apply_outputs(sector_);
  }

  emb::units::erad_f32 angle() const
  {
    return angle_;
  }
private:
  void apply_outputs(sector s)
  {
    auto const code = hall_code(s);
    for (auto i = 0uz; i < 3; ++i) {
      (code & (1u << i)) ? outputs_[i].set() : outputs_[i].reset();
    }
  }
};

} // namespace emb::hall
