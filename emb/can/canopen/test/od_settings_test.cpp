#include <cstdint>
#include <expected>
#include <string>
#include <string_view>

#include <emb/can/canopen/od_dictionary.hpp>
#include <emb/can/canopen/od_handlers.hpp>
#include <emb/can/canopen/od_settings.hpp>
#include <emb/settings/section.hpp>
#include <emb/test/mock/ram_storage.hpp>
#include <emb/units.hpp>

namespace {

using namespace emb::can::canopen;
using enum od_data_type;
namespace settings = emb::settings;

inline constexpr auto drive_phase_swap =
    settings::param("drive.phase_swap",
                    false,
                    {.apply = settings::apply_policy::on_command});
inline constexpr auto motor_p =
    settings::param("motor.p",
                    std::int32_t{11},
                    {.min = std::int32_t{1}, .max = std::int32_t{64}});
inline constexpr auto drive_freq =
    settings::param("drive.freq",
                    emb::units::hz_f32{10000.0f},
                    {.min = emb::units::hz_f32{1000.0f},
                     .max = emb::units::hz_f32{20000.0f},
                     .apply = settings::apply_policy::on_command});
inline constexpr auto prod_serial =
    settings::param("prod.serial", std::uint32_t{0}, {.writable = false});
inline constexpr auto prod_secret =
    settings::param("prod.secret", std::uint32_t{0}, {.expose = false});

inline constexpr auto schema = settings::make_schema(drive_phase_swap,
                                                     motor_p,
                                                     drive_freq,
                                                     prod_serial,
                                                     prod_secret);

using fram = emb::test::ram_storage<512>;
inline constexpr settings::placement fram_placement{.magic = 0x47464354u,
                                                    .base = 0,
                                                    .slot_capacity = 128,
                                                    .slot_count = 2};

using test_section = settings::section<schema, fram, fram_placement>;

// An applier that checks nothing and admits every change unless `busy`.
struct test_applier {
  bool busy = false;

  struct guard {};

  constexpr bool check(settings::image<schema> const&)
  {
    return true;
  }

  constexpr guard lock()
  {
    return {};
  }

  constexpr bool admit(settings::image<schema> const&,
                       settings::image<schema> const&)
  {
    return !busy;
  }

  constexpr void configure(settings::image<schema> const&,
                           settings::image<schema> const&)
  {
  }
};

// The context of the dictionary holds the section, so that the bridge can
// be followed through in constant evaluation.
struct ctx {
  fram memory;
  test_section section;
  bool busy = false;
  float actual = 9500.0f;
};

constexpr test_section& section_of(ctx& c)
{
  return c.section;
}

constexpr test_applier make_applier(ctx& c)
{
  return test_applier{c.busy};
}

constexpr float actual_freq(ctx const& c)
{
  return c.actual;
}

constexpr std::int32_t actual_pole_pairs()
{
  return 11;
}

using bridge = od_settings<section_of, make_applier>;

// -- Error mapping --

static_assert(to_sdo_abort(settings::error::unknown_parameter)
              == sdo_abort_code::object_not_found);
static_assert(to_sdo_abort(settings::error::read_only)
              == sdo_abort_code::write_to_read_only);
static_assert(to_sdo_abort(settings::error::type_mismatch)
              == sdo_abort_code::data_type_mismatch);
static_assert(to_sdo_abort(settings::error::out_of_range)
              == sdo_abort_code::value_range_exceeded);

static_assert(to_sdo_abort(settings::apply_refusal::unknown_parameter)
              == sdo_abort_code::object_not_found);
static_assert(to_sdo_abort(settings::apply_refusal::not_applicable)
              == sdo_abort_code::unsupported_access);
static_assert(to_sdo_abort(settings::apply_refusal::invalid)
              == sdo_abort_code::parameter_incompatibility);
static_assert(to_sdo_abort(settings::apply_refusal::not_admitted)
              == sdo_abort_code::state_error);

static_assert(to_sdo_abort(settings::save_refusal::unapplied)
              == sdo_abort_code::state_error);
static_assert(to_sdo_abort(settings::save_refusal::invalid)
              == sdo_abort_code::parameter_incompatibility);
static_assert(to_sdo_abort(settings::save_refusal::medium)
              == sdo_abort_code::data_store_error);

static_assert(detail::to_settings_value(od_value{1.5f})
              == settings::value{1.5f});
static_assert(!detail::to_settings_value(od_value{std::int8_t{7}}));

static_assert(detail::not_exposed_message<"prod.secret">().view()
              == "od: settings parameter 'prod.secret' is not exposed");
static_assert(detail::actual_type_message<"drive.freq">().view()
              == "od: the actual value of settings parameter 'drive.freq' is "
                 "not read as its type");

// -- Catalogs --

static_assert(bridge::catalog.what == "settings parameter");
static_assert(bridge::catalog.names.size() == 5);
static_assert(bridge::catalog.names[1] == "motor.p");
static_assert(bridge::catalog.exposed[3]);
static_assert(!bridge::catalog.exposed[4]);

// -- Bindings --

template<auto Binding>
consteval od_binding<ctx> bind()
{
  return Binding;
}

static_assert(bind<bridge::param<drive_phase_swap>>().type == boolean);
static_assert(bind<bridge::param<motor_p>>().type == int32);
static_assert(bind<bridge::param<drive_freq>>().type == float32);
static_assert(bind<bridge::param<motor_p>>().arg == 1);
static_assert(bind<bridge::param<motor_p>>().catalog == &bridge::catalog);
static_assert(bind<bridge::param<motor_p>>().read == nullptr);
static_assert(bind<bridge::param<motor_p>>().layout.size() == 10);
static_assert(bind<bridge::param<drive_freq, actual_freq>>().layout.size()
              == 11);
static_assert(bind<bridge::param<motor_p, actual_pole_pairs>>().layout.size()
              == 11);
static_assert(!bind<bridge::param<motor_p>>().diagnosed);

// A parameter a protocol may not write shows its value read-only.
static_assert(bind<bridge::param<prod_serial>>().layout[1].access
              == od_access::ro);
static_assert(bind<bridge::param<prod_serial>>().layout[1].write == nullptr);
static_assert(bind<bridge::param<motor_p>>().layout[1].access == od_access::rw);

// -- Records --

// clang-format off
inline constexpr od_row<ctx> records[] = {
    {{0x3100, 0x00}, "config", "drive",    "phase_swap",   "",   boolean, bridge::param<drive_phase_swap>},
    {{0x3101, 0x00}, "config", "motor",    "pole_pairs",   "",   int32,   bridge::param<motor_p>},
    {{0x3102, 0x00}, "config", "drive",    "freq",         "Hz", float32, bridge::param<drive_freq, actual_freq>},
    {{0x3103, 0x00}, "config", "prod",     "serial",       "",   uint32,  bridge::param<prod_serial>},
    {{0x3001, 0x01}, "ctl",    "settings", "apply_all",    "",   exec,    od_exec<bridge::apply_all>},
    {{0x3001, 0x02}, "ctl",    "settings", "discard_all",  "",   exec,    od_exec<bridge::discard_all>},
    {{0x3001, 0x03}, "ctl",    "settings", "last_failure", "",   uint32,  od_ro<bridge::last_failure>},
};
// clang-format on

inline constexpr auto dictionary = make_dictionary<records>();

// Reads and writes the dictionary as a server would, through `c`.
struct client {
  od_view<ctx> view;
  ctx& c;

  constexpr od_read_result read(od_key key)
  {
    auto const member = view.find(key);
    if (!member) return std::unexpected(member.error());
    return member->read(c, member->arg);
  }

  constexpr od_write_result write(od_key key, od_value value)
  {
    auto const member = view.find(key);
    if (!member) return std::unexpected(member.error());
    return member->write(c, member->arg, value);
  }

  constexpr od_write_result exec(od_key key)
  {
    return write(key, od_value{std::uint32_t{0}});
  }
};

// A record shows a parameter whole: its value, applied and stored, its
// default and bounds, its policy, and its actual value if it has one.
consteval bool test_a_record_shows_its_parameter()
{
  ctx c;
  auto _ = c.section.load(c.memory);
  client od{dictionary, c};

  if (od.read({0x3101, 0x00}) != od_read_result{std::uint8_t{0x09}}) {
    return false;
  }
  if (od.read({0x3102, 0x00}) != od_read_result{std::uint8_t{0x0A}}) {
    return false;
  }
  if (od.read({0x3101, 0x01}) != od_read_result{std::int32_t{11}}) return false;
  if (od.read({0x3101, 0x02}) != od_read_result{std::int32_t{11}}) return false;
  if (od.read({0x3101, 0x03}) != od_read_result{std::int32_t{11}}) return false;
  if (od.read({0x3101, 0x04}) != od_read_result{std::int32_t{11}}) return false;
  if (od.read({0x3101, 0x05}) != od_read_result{std::int32_t{1}}) return false;
  if (od.read({0x3101, 0x06}) != od_read_result{std::int32_t{64}}) return false;
  if (od.read({0x3102, 0x05}) != od_read_result{1000.0f}) return false;
  if (od.read({0x3100, 0x07}) != od_read_result{std::uint8_t{0}}) return false;
  if (od.read({0x3101, 0x07}) != od_read_result{std::uint8_t{1}}) return false;
  if (od.read({0x3102, 0x0A}) != od_read_result{9500.0f}) return false;

  auto const no_subindex = std::unexpected(sdo_abort_code::subindex_not_found);
  if (od.read({0x3101, 0x0A}) != no_subindex) return false;
  if (od.read({0x3102, 0x0B}) != no_subindex) return false;

  // A parameter a protocol may not write shows its value read-only.
  auto const serial = od.view.find({0x3103, 0x01});
  return serial
      && (serial->access == od_access::ro)
      && (serial->write == nullptr);
}

// A value written waits until applied: the record applies it, or discards
// it, and a refusal answers with its abort and is kept as the last failure.
consteval bool test_a_record_applies_its_parameter()
{
  ctx c;
  auto _ = c.section.load(c.memory);
  client od{dictionary, c};

  if (!od.write({0x3102, 0x01}, od_value{12000.0f})) return false;
  if (od.read({0x3102, 0x01}) != od_read_result{12000.0f}) return false;
  if (od.read({0x3102, 0x02}) != od_read_result{10000.0f}) return false;
  if (!c.section.unapplied()) return false;

  auto const out_of_range = od.write({0x3102, 0x01}, od_value{30000.0f});
  if (out_of_range != std::unexpected(sdo_abort_code::value_range_exceeded)) {
    return false;
  }
  auto const narrow = od.write({0x3102, 0x01}, od_value{std::int8_t{1}});
  if (narrow != std::unexpected(sdo_abort_code::data_type_mismatch)) {
    return false;
  }

  c.busy = true;
  if (od.exec({0x3102, 0x08}) != std::unexpected(sdo_abort_code::state_error)) {
    return false;
  }
  if (od.read({0x3001, 0x03}) != od_read_result{std::uint32_t{0x08000022}}) {
    return false;
  }

  c.busy = false;
  if (!od.exec({0x3102, 0x08})) return false;
  if (od.read({0x3102, 0x02}) != od_read_result{12000.0f}) return false;
  if (c.section.unapplied()) return false;

  // A parameter applied after a restart is not applied on command.
  if (!od.write({0x3101, 0x01}, od_value{std::int32_t{7}})) return false;
  auto const not_applicable = od.exec({0x3101, 0x08});
  if (not_applicable != std::unexpected(sdo_abort_code::unsupported_access)) {
    return false;
  }
  if (od.read({0x3001, 0x03}) != od_read_result{std::uint32_t{0x06010000}}) {
    return false;
  }
  if (!od.exec({0x3101, 0x09})) return false;
  if (od.read({0x3101, 0x01}) != od_read_result{std::int32_t{11}}) return false;

  // Nor is one a protocol may not write put back.
  return od.exec({0x3103, 0x09})
      == std::unexpected(sdo_abort_code::write_to_read_only);
}

// The commands of the bridge apply and discard every value.
consteval bool test_commands()
{
  ctx c;
  auto _ = c.section.load(c.memory);
  client od{dictionary, c};

  if (!od.write({0x3100, 0x01}, od_value{true})) return false;
  if (!od.write({0x3101, 0x01}, od_value{std::int32_t{7}})) return false;
  if (!od.exec({0x3001, 0x01})) return false;
  if (od.read({0x3100, 0x02}) != od_read_result{true}) return false;
  // A parameter applied after a restart keeps waiting for it.
  if (od.read({0x3101, 0x02}) != od_read_result{std::int32_t{11}}) return false;

  if (!od.write({0x3102, 0x01}, od_value{15000.0f})) return false;
  if (!od.exec({0x3001, 0x02})) return false;
  if (od.read({0x3102, 0x01}) != od_read_result{10000.0f}) return false;
  return od.read({0x3101, 0x01}) == od_read_result{std::int32_t{11}};
}

static_assert(test_a_record_shows_its_parameter());
static_assert(test_a_record_applies_its_parameter());
static_assert(test_commands());

// -- Coverage --

consteval std::string check_missing()
{
  // clang-format off
  static constexpr od_row<ctx> rows[] = {
      {{0x3100, 0x00}, "config", "drive", "phase_swap", "", boolean, bridge::param<drive_phase_swap>},
  };
  // clang-format on
  return detail::od_check(rows);
}

static_assert(check_missing() == "od: settings parameter 'motor.p' has no row");

// A row built by hand around the builder's back.
consteval std::string check_hidden()
{
  static constexpr od_binding<ctx> secret{
      .access = od_access::ro,
      .type = uint32,
      .arg = 4,
      .catalog = &bridge::catalog,
      .layout = bind<bridge::param<prod_serial>>().layout};
  // clang-format off
  static constexpr od_row<ctx> rows[] = {
      {{0x3100, 0x00}, "config", "drive",  "phase_swap", "",   boolean, bridge::param<drive_phase_swap>},
      {{0x3101, 0x00}, "config", "motor",  "pole_pairs", "",   int32,   bridge::param<motor_p>},
      {{0x3102, 0x00}, "config", "drive",  "freq",       "Hz", float32, bridge::param<drive_freq>},
      {{0x3103, 0x00}, "config", "prod",   "serial",     "",   uint32,  bridge::param<prod_serial>},
      {{0x3104, 0x00}, "config", "prod",   "secret",     "",   uint32,  secret},
  };
  // clang-format on
  return detail::od_check(rows);
}

static_assert(check_hidden()
              == "od: settings parameter 'prod.secret' is not exposed but has "
                 "a row: 3104h:00 config/prod/secret");

// A row states the parameter's type; a wrong one is reported at the row.
consteval std::string check_declared()
{
  // clang-format off
  static constexpr od_row<ctx> rows[] = {
      {{0x3101, 0x00}, "config", "motor", "pole_pairs", "", float32, bridge::param<motor_p>},
  };
  // clang-format on
  return detail::od_check(rows);
}

static_assert(check_declared()
              == "od: 3101h:00 config/motor/pole_pairs: declared float32, "
                 "bound as int32");

} // namespace
