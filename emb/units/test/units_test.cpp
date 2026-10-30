#include <emb/i18n.hpp>
#include <emb/meta/unit_symbol.hpp>
#include <emb/units.hpp>

#include <cassert>
#include <string_view>

namespace {

template<typename Unit>
constexpr Unit templated_conversion()
{
  return emb::units::convert_to<Unit>(
      emb::units::erad_f32{std::numbers::pi_v<float>});
}

constexpr bool test_units_conversion()
{
  using namespace emb::units;

  float v1{90.0f};
  deg_f32 deg1{v1};
  assert(convert_to<deg_f32>(deg1) == deg1);
  assert(convert_to<rad_f32>(deg1) == rad_f32{emb::to_rad(v1)});

  assert(emb::norm360(deg_f32{380.0f}) == deg_f32(20.0f));
  assert(emb::norm360(deg_f32{-30.0f}) == deg_f32(330.0f));
  // a tiny negative input must not round onto the upper bound
  assert(emb::norm360(deg_f32{-1e-6f}) == deg_f32(0.0f));
  assert(emb::norm360(deg_f32{-1.5e-5f}).value < 360.0f);
  // a negative multiple of 360 must give +0, not -0
  assert(std::bit_cast<std::uint32_t>(emb::norm360(deg_f32{-360.0f}).value)
         == 0u);

  assert(emb::norm180(deg_f32{200.0f}) == deg_f32(-160.0f));
  assert(emb::norm180(deg_f32{-10.0f}) == deg_f32(-10.0f));
  assert(emb::norm180(deg_f32{-180.00002f}) == deg_f32(-180.0f));

  [[maybe_unused]] constexpr auto near = [](float a, float b) {
    return (a - b) < 1e-4f && (b - a) < 1e-4f;
  };

  // norm360_fast
  assert(near(emb::norm360_fast(deg_f32{380.0f}).value, 20.0f));
  assert(near(emb::norm360_fast(deg_f32{-30.0f}).value, 330.0f));
  assert(near(emb::norm360_fast(deg_f32{360.0f}).value, 0.0f));
  assert(near(emb::norm360_fast(deg_f32{720.0f}).value, 0.0f));
  // a large negative input must not land on the upper bound
  assert(emb::norm360_fast(deg_f32{-7000000512.0f}).value < 360.0f);

  // norm180_fast
  assert(near(emb::norm180_fast(deg_f32{200.0f}).value, -160.0f));
  assert(near(emb::norm180_fast(deg_f32{-10.0f}).value, -10.0f));
  assert(emb::norm180_fast(deg_f32{-7000000512.0f}).value < 180.0f);

  float v2{3000.0f};
  [[maybe_unused]] std::int32_t p{4};
  rpm_f32 rpm1{v2};
  assert(convert_to<rpm_f32>(rpm1) == rpm1);
  assert(convert_to<eradps_f32>(rpm1, p) == eradps_f32{emb::to_eradps(v2, p)});

  assert(templated_conversion<erad_f32>()
         == erad_f32{std::numbers::pi_v<float>});

  assert(templated_conversion<edeg_f32>()
         == edeg_f32{emb::to_deg(std::numbers::pi_v<float>)});

  [[maybe_unused]] hz_f32 freq{100};
  assert(1 / freq == sec_f32{1.0f / 100.0f});

  [[maybe_unused]] sec_f32 per{0.0001f};
  assert(1 / per == hz_f32(1 / 0.0001f));

  // sec -> chrono duration
  assert(to_duration<std::chrono::milliseconds>(sec_f32{1.5f})
         == std::chrono::milliseconds{1500});
  assert(to_duration<std::chrono::microseconds>(sec_f32{0.25f})
         == std::chrono::microseconds{250000});
  assert(to_milliseconds(sec_f32{0.125f}) == std::chrono::milliseconds{125});

  // truncates towards zero, as a cast to a coarser unit does
  assert(to_milliseconds(sec_f32{0.0015f}) == std::chrono::milliseconds{1});
  assert(to_milliseconds(sec_f32{-0.0015f}) == std::chrono::milliseconds{-1});

  // abs
  assert(abs(rpm_f32{-100.0f}) == rpm_f32{100.0f});
  assert(abs(rpm_f32{100.0f}) == rpm_f32{100.0f});
  assert(abs(rpm_f32{0.0f}) == rpm_f32{0.0f});
  assert(abs(deg_f32{-90.0f}) > deg_f32{45.0f});

  // value_of
  assert(value_of(rpm_f32{-100.0f}) == -100.0f);

  return true;
}

static_assert(test_units_conversion());

// A quantity is a structural type, so it can be a template argument.
template<auto v>
constexpr auto template_argument = v;

static_assert(template_argument<emb::units::volt_f32{1.5f}>
              == emb::units::volt_f32{1.5f});

// -- Unit symbols --

using text = emb::i18n::text;

// Each unit tag names the symbol of its unit in each language, and a quantity
// returns it through unit_symbol, found by argument-dependent lookup.
static_assert(unit_symbol(emb::units::volt_f32{}) == text{"V", "В"});
static_assert(emb::unit_with_symbol<emb::units::volt_f32>);

static_assert(emb::unit_symbol_of<emb::units::hz_f32>() == text{"Hz", "Гц"});
static_assert(emb::unit_symbol_of<emb::units::sec_f32>() == text{"s", "с"});
static_assert(emb::unit_symbol_of<emb::units::microsiemens_per_cm_f32>()
              == text{"µS/cm", "мкСм/см"});
static_assert(emb::unit_symbol_of<emb::units::cubic_meter_per_hour_f32>()
              == text{"m³/h", "м³/ч"});
static_assert(emb::unit_symbol_of<emb::units::liter_per_minute_f32>()
              == text{"L/min", "л/мин"});
static_assert(emb::unit_symbol_of<emb::units::amp_f32>() == text{"A", "А"});
static_assert(emb::unit_symbol_of<emb::units::volt_f32>() == text{"V", "В"});
static_assert(emb::unit_symbol_of<emb::units::ohm_f32>() == text{"Ω", "Ом"});
static_assert(emb::unit_symbol_of<emb::units::erad_f32>()
              == text{"rad(el)", "рад(эл)"});
static_assert(emb::unit_symbol_of<emb::units::edeg_f32>()
              == text{"°el", "°эл"});
static_assert(emb::unit_symbol_of<emb::units::rad_f32>() == text{"rad", "рад"});
static_assert(emb::unit_symbol_of<emb::units::deg_f32>() == text{"°", "°"});
static_assert(emb::unit_symbol_of<emb::units::pascal_f32>()
              == text{"Pa", "Па"});
static_assert(emb::unit_symbol_of<emb::units::megapascal_f32>()
              == text{"MPa", "МПа"});
static_assert(emb::unit_symbol_of<emb::units::atmosphere_f32>()
              == text{"atm", "атм"});
static_assert(emb::unit_symbol_of<emb::units::degree_celsius_f32>()
              == text{"°C", "°C"});
static_assert(emb::unit_symbol_of<emb::units::kelvin_f32>() == text{"K", "К"});
static_assert(emb::unit_symbol_of<emb::units::rpm_f32>()
              == text{"rpm", "об/мин"});
static_assert(emb::unit_symbol_of<emb::units::eradps_f32>()
              == text{"rad(el)/s", "рад(эл)/с"});

// A text is compared by its bytes, so the symbols beyond ASCII are pinned to
// their code points: the ohm to U+03A9, not to its lookalike U+2126, the micro
// sign to U+00B5, not to U+03BC, the degree to U+00B0. Narrow literals are
// UTF-8: U+03A9 is the bytes CE A9.
static_assert(emb::units::tags::ohm::symbol.en == "\u03A9");
static_assert(emb::units::tags::ohm::symbol.en == "\xCE\xA9");
static_assert(emb::units::tags::microsiemens_per_cm::symbol.en == "\u00B5S/cm");
static_assert(emb::units::tags::cubic_meter_per_hour::symbol.en == "m\u00B3/h");
static_assert(emb::units::tags::deg::symbol.en == "\u00B0");
static_assert(emb::units::tags::edeg::symbol.en == "\u00B0el");
static_assert(emb::units::tags::degree_celsius::symbol.en == "\u00B0C");

// The Russian symbols are Cyrillic, also where a letter looks Latin: the
// ampere is U+0410, the kelvin U+041A, the second U+0441 and the mega- of the
// megapascal U+041C. The degree Celsius is the same text in each language,
// with the Latin C.
static_assert(emb::units::tags::amp::symbol.ru == "\u0410");
static_assert(emb::units::tags::kelvin::symbol.ru == "\u041A");
static_assert(emb::units::tags::sec::symbol.ru == "\u0441");
static_assert(emb::units::tags::megapascal::symbol.ru == "\u041C\u041F\u0430");
static_assert(emb::units::tags::degree_celsius::symbol.ru == "\u00B0C");

// A tag without a symbol leaves its unit without one.
struct untitled_unit {};
static_assert(
    !emb::unit_with_symbol<emb::units::quantity<float, untitled_unit>>);
static_assert(emb::unit_symbol_of<emb::units::quantity<float, untitled_unit>>()
              == text{});

// A tag whose symbol is in one language, a std::string_view, as before the
// symbols took one in each, leaves its unit without one too.
struct one_language_unit {
  static constexpr std::string_view symbol = "u";
};
static_assert(
    !emb::unit_with_symbol<emb::units::quantity<float, one_language_unit>>);

} // namespace
