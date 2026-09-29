#include <cstdint>
#include <expected>
#include <string>
#include <string_view>

#include <emb/can/canopen/od_dictionary.hpp>
#include <emb/can/canopen/od_handlers.hpp>
#include <emb/can/canopen/od_settings.hpp>
#include <emb/settings/image.hpp>
#include <emb/units.hpp>

namespace {

using namespace emb::can::canopen;
namespace settings = emb::settings;

struct ctx {};

inline constexpr auto schema = settings::make_schema(
    settings::param("drive.phase_swap", false),
    settings::param("motor.p",
                    std::int32_t{11},
                    {.min = std::int32_t{1}, .max = std::int32_t{64}}),
    settings::param("prot.timeout", std::uint32_t{1000}),
    settings::param("prot.uvp", 35.0f),
    settings::param("drive.freq", emb::units::hz_f32{10000.0f}),
    settings::param("prod.serial", std::uint32_t{0}, {.writable = false}),
    settings::param("prod.secret", std::uint32_t{0}, {.expose = false}));

// The accessors of a fresh image: every call starts from the defaults, which
// is enough to see what the bridge makes of their answers.
constexpr auto get_at(std::size_t index)
    -> std::expected<settings::value, settings::error>
{
  return settings::image<schema>{}.get_at(index);
}

constexpr auto set_at(std::size_t index, settings::value const& value)
    -> std::expected<settings::change, settings::error>
{
  settings::image<schema> values;
  return values.set_at(index, value);
}

constexpr auto restore_at(std::size_t index)
    -> std::expected<settings::change, settings::error>
{
  settings::image<schema> values;
  return values.restore_default_at(index);
}

using bridge = od_settings<schema, get_at, set_at, restore_at>;

// -- Error mapping --

static_assert(to_sdo_abort(settings::error::unknown_parameter)
              == sdo_abort_code::object_not_found);
static_assert(to_sdo_abort(settings::error::read_only)
              == sdo_abort_code::write_to_read_only);
static_assert(to_sdo_abort(settings::error::type_mismatch)
              == sdo_abort_code::data_type_mismatch);
static_assert(to_sdo_abort(settings::error::out_of_range)
              == sdo_abort_code::value_range_exceeded);

static_assert(detail::to_settings_value(od_value{1.5f})
              == settings::value{1.5f});
static_assert(detail::to_settings_value(od_value{std::uint32_t{7}})
              == settings::value{std::uint32_t{7}});
static_assert(!detail::to_settings_value(od_value{std::int8_t{7}}));
static_assert(!detail::to_settings_value(od_value{std::uint16_t{7}}));

static_assert(detail::not_exposed_message<"prod.secret">().view()
              == "od: settings parameter 'prod.secret' is not exposed");
static_assert(detail::not_writable_message<"prod.serial">().view()
              == "od: settings parameter 'prod.serial' is not writable; bind "
                 "it with ro");

// -- Catalog --

static_assert(bridge::catalog.what == "settings parameter");
static_assert(bridge::catalog.names.size() == 7);
static_assert(bridge::catalog.names[1] == "motor.p");
static_assert(bridge::catalog.exposed[5]);
static_assert(!bridge::catalog.exposed[6]);

// -- Bindings --

template<auto Binding>
consteval od_binding<ctx> bind()
{
  return Binding;
}

static_assert(bind<bridge::rw<"drive.phase_swap">>().type
              == od_value_type::boolean);
static_assert(bind<bridge::rw<"motor.p">>().type == od_value_type::int32);
static_assert(bind<bridge::rw<"prot.timeout">>().type == od_value_type::uint32);
static_assert(bind<bridge::rw<"prot.uvp">>().type == od_value_type::float32);
static_assert(bind<bridge::rw<"drive.freq">>().type == od_value_type::float32);

static_assert(bind<bridge::rw<"motor.p">>().access == od_access::rw);
static_assert(bind<bridge::rw<"motor.p">>().arg == 1);
static_assert(bind<bridge::rw<"motor.p">>().read == &bridge::read<ctx>);
static_assert(bind<bridge::rw<"motor.p">>().write == &bridge::write<ctx>);
static_assert(bind<bridge::rw<"motor.p">>().restore == &bridge::restore<ctx>);
static_assert(bind<bridge::rw<"motor.p">>().catalog == &bridge::catalog);
static_assert(!bind<bridge::rw<"motor.p">>().diagnosed);

static_assert(bind<bridge::ro<"prod.serial">>().access == od_access::ro);
static_assert(bind<bridge::ro<"prod.serial">>().arg == 5);
static_assert(bind<bridge::ro<"prod.serial">>().write == nullptr);
static_assert(bind<bridge::ro<"prod.serial">>().restore == nullptr);

// -- Access --

consteval bool test_access()
{
  ctx c;
  if (bridge::read(c, 1) != od_read_result{std::int32_t{11}}) return false;
  if (bridge::read(c, 4) != od_read_result{10000.0f}) return false;
  auto const unknown = bridge::read(c, 99);
  if (unknown || unknown.error() != sdo_abort_code::object_not_found) {
    return false;
  }

  if (!bridge::write(c, 1, od_value{std::int32_t{12}})) return false;

  auto const out_of_range = bridge::write(c, 1, od_value{std::int32_t{65}});
  if (out_of_range
      || out_of_range.error() != sdo_abort_code::value_range_exceeded) {
    return false;
  }
  auto const narrow = bridge::write(c, 1, od_value{std::int8_t{12}});
  if (narrow || narrow.error() != sdo_abort_code::data_type_mismatch) {
    return false;
  }
  auto const retyped = bridge::write(c, 1, od_value{std::uint32_t{12}});
  if (retyped || retyped.error() != sdo_abort_code::data_type_mismatch) {
    return false;
  }
  auto const read_only = bridge::write(c, 5, od_value{std::uint32_t{1}});
  if (read_only || read_only.error() != sdo_abort_code::write_to_read_only) {
    return false;
  }

  if (!bridge::restore(c, 1)) return false;
  auto const not_restored = bridge::restore(c, 5);
  return !not_restored
      && (not_restored.error() == sdo_abort_code::write_to_read_only);
}

static_assert(test_access());

// -- Dictionary --

consteval bool test_dictionary()
{
  // clang-format off
  static constexpr od_row<ctx> rows[] = {
      {{0x3002, 0x01}, "config", "drive", "phase_swap", "",   bridge::rw<"drive.phase_swap">},
      {{0x3004, 0x01}, "config", "motor", "pole_pairs", "",   bridge::rw<"motor.p">},
      {{0x3003, 0x0B}, "config", "prot",  "timeout",    "ms", bridge::rw<"prot.timeout">},
      {{0x3003, 0x01}, "config", "prot",  "uvp",        "V",  bridge::rw<"prot.uvp">},
      {{0x3002, 0x08}, "config", "drive", "freq",       "Hz", bridge::rw<"drive.freq">},
      {{0x3000, 0x01}, "config", "prod",  "serial",     "",   bridge::ro<"prod.serial">},
      {{0x1011, 0x04}, "ctl",    "sys",   "restore",    "",   od_restore_default},
  };
  // clang-format on

  auto const dictionary = make_dictionary<rows>();
  od_view<ctx> const view{dictionary};

  if (view.restore() != &bridge::restore<ctx>) return false;

  auto const* pole_pairs = view.find({0x3004, 0x01});
  if ((pole_pairs == nullptr) || !pole_pairs->restorable) return false;

  auto const* serial = view.find({0x3000, 0x01});
  if ((serial == nullptr) || serial->restorable) return false;

  ctx c;
  if (pole_pairs->read(c, pole_pairs->arg)
      != od_read_result{std::int32_t{11}}) {
    return false;
  }
  if (!pole_pairs->write(c, pole_pairs->arg, od_value{std::int32_t{3}})) {
    return false;
  }
  if (!view.restore()(c, pole_pairs->arg)) return false;
  return serial->read(c, serial->arg) == od_read_result{std::uint32_t{0}};
}

static_assert(test_dictionary());

// -- Coverage --

consteval std::string check_missing()
{
  // clang-format off
  static constexpr od_row<ctx> rows[] = {
      {{0x3002, 0x01}, "config", "drive", "phase_swap", "", bridge::rw<"drive.phase_swap">},
  };
  // clang-format on
  return detail::od_check(rows);
}

static_assert(check_missing() == "od: settings parameter 'motor.p' has no row");

consteval std::string check_twice()
{
  // clang-format off
  static constexpr od_row<ctx> rows[] = {
      {{0x3002, 0x01}, "config", "drive", "phase_swap", "",   bridge::rw<"drive.phase_swap">},
      {{0x3004, 0x01}, "config", "motor", "pole_pairs", "",   bridge::rw<"motor.p">},
      {{0x3003, 0x0B}, "config", "prot",  "timeout",    "ms", bridge::rw<"prot.timeout">},
      {{0x3003, 0x01}, "config", "prot",  "uvp",        "V",  bridge::rw<"prot.uvp">},
      {{0x3002, 0x08}, "config", "drive", "freq",       "Hz", bridge::rw<"drive.freq">},
      {{0x3000, 0x01}, "config", "prod",  "serial",     "",   bridge::ro<"prod.serial">},
      {{0x3002, 0x09}, "config", "drive", "swap",       "",   bridge::ro<"drive.phase_swap">},
  };
  // clang-format on
  return detail::od_check(rows);
}

static_assert(check_twice()
              == "od: settings parameter 'drive.phase_swap' has two rows: "
                 "3002h:01 config/drive/phase_swap and 3002h:09 "
                 "config/drive/swap");

// A row built by hand around the builder's back.
consteval std::string check_hidden()
{
  static constexpr od_binding<ctx> secret{.access = od_access::ro,
                                          .type = od_value_type::uint32,
                                          .read = &bridge::read<ctx>,
                                          .arg = 6,
                                          .catalog = &bridge::catalog};
  // clang-format off
  static constexpr od_row<ctx> rows[] = {
      {{0x3002, 0x01}, "config", "drive", "phase_swap", "",   bridge::rw<"drive.phase_swap">},
      {{0x3004, 0x01}, "config", "motor", "pole_pairs", "",   bridge::rw<"motor.p">},
      {{0x3003, 0x0B}, "config", "prot",  "timeout",    "ms", bridge::rw<"prot.timeout">},
      {{0x3003, 0x01}, "config", "prot",  "uvp",        "V",  bridge::rw<"prot.uvp">},
      {{0x3002, 0x08}, "config", "drive", "freq",       "Hz", bridge::rw<"drive.freq">},
      {{0x3000, 0x01}, "config", "prod",  "serial",     "",   bridge::ro<"prod.serial">},
      {{0x3000, 0x02}, "config", "prod",  "secret",     "",   secret},
  };
  // clang-format on
  return detail::od_check(rows);
}

static_assert(check_hidden()
              == "od: settings parameter 'prod.secret' is not exposed but has "
                 "a row: 3000h:02 config/prod/secret");

} // namespace
