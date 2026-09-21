#pragma once

#include <emb/units.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <numbers>
#include <optional>
#include <utility>

namespace emb::hall {

inline constexpr emb::units::erad_f32 sector_width{std::numbers::pi_v<float>
                                                   / 3.0f};

inline constexpr emb::units::erad_f32 sector_halfwidth{std::numbers::pi_v<float>
                                                       / 6.0f};

enum class sector : std::uint32_t { a, ab, b, bc, c, ca };

constexpr std::uint8_t hall_code(sector s)
{
  static constexpr std::array<std::uint8_t, 6> codes =
      {0b001, 0b011, 0b010, 0b110, 0b100, 0b101};
  return codes[std::to_underlying(s)];
}

constexpr std::optional<sector> decode_sector(std::uint8_t v)
{
  switch (v) {
  case 0b001: return sector::a;
  case 0b011: return sector::ab;
  case 0b010: return sector::b;
  case 0b110: return sector::bc;
  case 0b100: return sector::c;
  case 0b101: return sector::ca;
  default: return std::nullopt;
  }
}

static_assert(decode_sector(hall_code(sector::a)) == sector::a);
static_assert(decode_sector(hall_code(sector::ab)) == sector::ab);
static_assert(decode_sector(hall_code(sector::b)) == sector::b);
static_assert(decode_sector(hall_code(sector::bc)) == sector::bc);
static_assert(decode_sector(hall_code(sector::c)) == sector::c);
static_assert(decode_sector(hall_code(sector::ca)) == sector::ca);

template<typename T>
class sector_map {
public:
  using container = std::array<T, 6>;

  constexpr sector_map() = default;

  constexpr explicit sector_map(container const& values) : values_(values) {}

  constexpr T operator[](sector const& sector) const
  {
    return values_[std::to_underlying(sector)];
  }

  constexpr T& operator[](sector const& sector)
  {
    return values_[std::to_underlying(sector)];
  }

  constexpr container& data()
  {
    return values_;
  }

  constexpr container const& data() const
  {
    return values_;
  }
private:
  container values_;
};

struct sector_span {
  emb::units::erad_f32 entry;
  emb::units::erad_f32 width;
};

enum class direction { fwd, rev };

constexpr sector_map<emb::units::edeg_f32>
compute_sector_widths(sector_map<emb::units::edeg_f32> const& entries,
                      direction dir)
{
  using T = emb::units::edeg_f32;
  using pair = std::pair<T, sector>;
  std::array<pair, 6> sorted;
  for (auto i = 0uz; i < 6; ++i) {
    sector s = static_cast<sector>(i);
    sorted[i] = {emb::norm360(entries[s]), s};
  }
  std::sort(sorted.begin(), sorted.end());

  sector_map<T> widths;
  for (auto i = 0uz; i < 6; ++i) {
    T gap = (dir == direction::fwd)
              ? sorted[(i + 1) % 6].first - sorted[i].first
              : sorted[i].first - sorted[(i + 6 - 1) % 6].first;
    if (gap.value() <= 0) {
      gap = T{gap.value() + 360.0f};
    }
    widths[sorted[i].second] = gap;
  }
  return widths;
}

class transition_sign_map {
public:
  using container = std::array<std::array<float, 6>, 6>;

  constexpr transition_sign_map() = default;

  constexpr explicit transition_sign_map(container const& signs) : signs_(signs)
  {
  }

  constexpr explicit transition_sign_map(
      sector_map<emb::units::edeg_f32> const& angles)
  {
    using angle_sector_pair = std::pair<emb::units::edeg_f32, sector>;
    // sorted array of pairs [angle; sector]
    std::array<angle_sector_pair, 6> sectors;

    for (auto i = 0uz; i < 6; ++i) {
      sector s = static_cast<sector>(i);
      sectors[i].first = emb::norm360(angles[s]);
      sectors[i].second = s;
    }

    std::sort(sectors.begin(), sectors.end());

    // here we get for example:
    // {30, ab}, {90, b}, {150, bc}, {210, c}, {270, ca}, {330, a}
    //   from      from      to         from      from       from
    //   '+'       '+'       ^          '-'       '-'        '0'

    for (auto i = 0uz; i < 6; ++i) {
      sector sector_to = sectors[i].second;
      this->at(sector_to, sectors[i].second) = 0.0f;
      this->at(sector_to, sectors[(i + 1) % 6].second) = -1.0f;
      this->at(sector_to, sectors[(i + 2) % 6].second) = -1.0f;
      this->at(sector_to, sectors[(i + 3) % 6].second) = 0.0f;
      this->at(sector_to, sectors[(i + 4) % 6].second) = 1.0f;
      this->at(sector_to, sectors[(i + 5) % 6].second) = 1.0f;
    }
  }

  constexpr float at(sector const& to, sector const& from) const
  {
    return signs_[std::to_underlying(to)][std::to_underlying(from)];
  }

  constexpr float& at(sector const& to, sector const& from)
  {
    return signs_[std::to_underlying(to)][std::to_underlying(from)];
  }

  constexpr auto operator<=>(transition_sign_map const&) const = default;
private:
  container signs_;
};

namespace detail {

inline constexpr sector_map<emb::units::edeg_f32> nominal_entries{
    {emb::units::edeg_f32(-30),
     emb::units::edeg_f32(30),
     emb::units::edeg_f32(90),
     emb::units::edeg_f32(150),
     emb::units::edeg_f32(210),
     emb::units::edeg_f32(270)}};

inline constexpr transition_sign_map
    nominal_signs({{{0.0f, -1.0f, -1.0f, 0.0f, 1.0f, 1.0f},
                    {1.0f, 0.0f, -1.0f, -1.0f, 0.0f, 1.0f},
                    {1.0f, 1.0f, 0.0f, -1.0f, -1.0f, 0.0f},
                    {0.0f, 1.0f, 1.0f, 0.0f, -1.0f, -1.0f},
                    {-1.0f, 0.0f, 1.0f, 1.0f, 0.0f, -1.0f},
                    {-1.0f, -1.0f, 0.0f, 1.0f, 1.0f, 0.0f}}});

inline constexpr sector_map<emb::units::edeg_f32> uniform_60{
    {emb::units::edeg_f32(60),
     emb::units::edeg_f32(60),
     emb::units::edeg_f32(60),
     emb::units::edeg_f32(60),
     emb::units::edeg_f32(60),
     emb::units::edeg_f32(60)}};

static_assert(transition_sign_map(nominal_entries) == nominal_signs);

static_assert(compute_sector_widths(nominal_entries, direction::fwd).data()
              == uniform_60.data());

static_assert(compute_sector_widths(nominal_entries, direction::rev).data()
              == uniform_60.data());

} // namespace detail

} // namespace emb::hall
