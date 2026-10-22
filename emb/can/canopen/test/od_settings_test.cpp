#include <cstdint>
#include <expected>
#include <span>
#include <string>
#include <string_view>

#include <emb/can/canopen/od_description.hpp>
#include <emb/can/canopen/od_dictionary.hpp>
#include <emb/can/canopen/od_handlers.hpp>
#include <emb/can/canopen/od_settings.hpp>
#include <emb/meta/enum_labels.hpp>
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
                    {.title = {"Phase swap", "Перестановка фаз"},
                     .apply = settings::apply_policy::on_command});
inline constexpr auto motor_p =
    settings::param("motor.p",
                    std::int32_t{11},
                    {.title = {"Pole pairs", "Число пар полюсов"},
                     .min = std::int32_t{1},
                     .max = std::int32_t{64}});
inline constexpr auto drive_freq =
    settings::param("drive.freq",
                    emb::units::hz_f32{10000.0f},
                    {.title = {"Frequency", "Частота"},
                     .min = emb::units::hz_f32{1000.0f},
                     .max = emb::units::hz_f32{20000.0f},
                     .apply = settings::apply_policy::on_command});
inline constexpr auto prod_serial = settings::param(
    "prod.serial",
    std::uint32_t{0},
    {.title = {"Serial number", "Серийный номер"}, .writable = false});
inline constexpr auto prod_secret =
    settings::param("prod.secret", std::uint32_t{0}, {.expose = false});
// A slope held as rpm, whose unit replaces the symbol of its type.
inline constexpr auto drive_slope =
    settings::param("drive.slope",
                    emb::units::rpm_f32{100.0f},
                    {.title = {"Speed slope", "Темп изменения скорости"},
                     .unit = "rpm/s",
                     .apply = settings::apply_policy::on_command});

inline constexpr auto schema = settings::make_schema(drive_phase_swap,
                                                     motor_p,
                                                     drive_freq,
                                                     prod_serial,
                                                     prod_secret,
                                                     drive_slope);

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

constexpr emb::units::hz_f32 actual_hz()
{
  return emb::units::hz_f32{9500.0f};
}

constexpr emb::units::rpm_f32 actual_rpm()
{
  return emb::units::rpm_f32{100.0f};
}

constexpr emb::units::eradps_f32 actual_eradps()
{
  return emb::units::eradps_f32{10.0f};
}

// A mode of the drive, labelled for a host, that travels as a uint32.
enum class drive_mode : std::uint32_t { idle, run };

consteval std::span<emb::enum_label<drive_mode> const> enum_labels(drive_mode)
{
  static constexpr emb::enum_label<drive_mode> labels[] = {
      {drive_mode::idle, "idle", {"Idle", "Ожидание"}},
      {drive_mode::run, "run", {"Run", "Работа"}},
  };
  return labels;
}

constexpr drive_mode actual_mode()
{
  return drive_mode::run;
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
static_assert(detail::actual_unit_message<"drive.freq">().view()
              == "od: the actual value of settings parameter 'drive.freq' is "
                 "read in another unit");

// The actual value is read as the type of its parameter: as the scalar it
// travels as, whatever wraps it, or as that very enumeration where either is
// one. A reader of an enumeration fits no parameter of its carrier.
static_assert(detail::od_reads_as<actual_freq, emb::units::hz_f32>());
static_assert(detail::od_reads_as<actual_hz, float>());
static_assert(detail::od_reads_as<actual_pole_pairs, std::int32_t>());
static_assert(!detail::od_reads_as<actual_freq, std::int32_t>());
static_assert(!detail::od_reads_as<actual_mode, std::uint32_t>());

// The actual value is read in the unit of its parameter: in a type that names
// no unit, in the type of the parameter, or in the parameter's unit, which
// may replace the symbol of the type.
static_assert(detail::od_reads_in_unit<actual_freq, emb::units::hz_f32>("Hz"));
static_assert(detail::od_reads_in_unit<actual_hz, emb::units::hz_f32>("Hz"));
static_assert(detail::od_reads_in_unit<actual_hz, float>("Hz"));
static_assert(
    detail::od_reads_in_unit<actual_rpm, emb::units::rpm_f32>("rpm/s"));
static_assert(!detail::od_reads_in_unit<actual_rpm, float>("Hz"));
static_assert(
    !detail::od_reads_in_unit<actual_eradps, emb::units::rpm_f32>("rpm"));

// -- Catalogs --

static_assert(bridge::catalog.what == "settings parameter");
static_assert(bridge::catalog.names.size() == 6);
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

// A record is named as its parameter: by the parameter's name and title.
static_assert(bind<bridge::param<motor_p>>().label.path == "motor.p");
static_assert(bind<bridge::param<motor_p>>().label.title == motor_p.title);
static_assert(bind<bridge::param<motor_p>>().label.unit.empty());

// The record's unit is the parameter's: the symbol of its type, or the unit
// that replaces it.
static_assert(bind<bridge::param<drive_freq>>().unit == "Hz");
static_assert(bind<bridge::param<motor_p>>().unit.empty());
static_assert(bind<bridge::param<drive_slope>>().unit == "rpm/s");
static_assert(!bind<bridge::param<drive_freq, actual_hz>>().diagnosed);
static_assert(!bind<bridge::param<drive_slope, actual_rpm>>().diagnosed);

// -- Records --

inline constexpr od_area areas[] = {
    {0x3000, 0x30FF, od_class::control},
    {0x3100, 0x31FF, od_class::parameters},
};

inline constexpr od_group groups[] = {
    {"drive", {"Drive", "Привод"}},
    {"motor", {"Motor", "Двигатель"}},
    {"prod", {"Production", "Производство"}},
    {"settings", {"Settings", "Настройки"}},
};

// clang-format off
inline constexpr od_row<ctx> records[] = {
    {{0x3100, 0x00}, {},                                                             bridge::param<drive_phase_swap>},
    {{0x3101, 0x00}, {},                                                             bridge::param<motor_p>},
    {{0x3102, 0x00}, {},                                                             bridge::param<drive_freq, actual_freq>},
    {{0x3103, 0x00}, {},                                                             bridge::param<prod_serial>},
    {{0x3104, 0x00}, {},                                                             bridge::param<drive_slope, actual_rpm>},
    {{0x3001, 0x01}, {"settings.apply_all", {"Apply all", "Применить все"}},         od_exec<bridge::apply_all>},
    {{0x3001, 0x02}, {"settings.discard_all", {"Discard all", "Отбросить все"}},     od_exec<bridge::discard_all>},
    {{0x3001, 0x03}, {"settings.last_refusal", {"Last refusal", "Последний отказ"}}, od_ro<bridge::last_refusal>},
};
// clang-format on

inline constexpr auto dictionary = make_dictionary<records, areas, groups>();

// -- Description --

inline constexpr auto description =
    make_description<records, areas, groups, "Test device">();

consteval std::string_view as_view(auto const& chars)
{
  return {chars.data(), chars.size()};
}

// Returns the number of times `part` occurs in `text`.
consteval std::size_t occurrences(std::string_view text, std::string_view part)
{
  auto count = 0uz;
  for (auto at = text.find(part); at != std::string_view::npos;
       at = text.find(part, at + 1)) {
    ++count;
  }
  return count;
}

static_assert(od_description_fingerprint(description)
              == dictionary.fingerprint());

// Three layouts, whatever the types of the parameters: writable without the
// actual value, writable with it, and read-only.
static_assert(occurrences(as_view(description), R"("type":"field")")
              == 10 + 11 + 10);
static_assert(
    as_view(description)
        .contains(
            R"({"type":"record","index":"0x3100","path":"drive.phase_swap",)"
            R"("title":"Phase swap","title_ru":"Перестановка фаз",)"
            R"("class":"parameters","datatype":"boolean","layout":0})"
            "\n"));
static_assert(
    as_view(description)
        .contains(R"({"type":"record","index":"0x3101","path":"motor.p",)"
                  R"("title":"Pole pairs","title_ru":"Число пар полюсов",)"
                  R"("class":"parameters","datatype":"int32","layout":0})"
                  "\n"));
static_assert(
    as_view(description)
        .contains(
            R"({"type":"record","index":"0x3102","path":"drive.freq",)"
            R"("title":"Frequency","title_ru":"Частота","class":"parameters",)"
            R"("datatype":"float32","unit":"Hz","layout":1})"
            "\n"));
static_assert(
    as_view(description)
        .contains(R"({"type":"record","index":"0x3103","path":"prod.serial",)"
                  R"("title":"Serial number","title_ru":"Серийный номер",)"
                  R"("class":"parameters","datatype":"uint32","layout":2})"
                  "\n"));
static_assert(
    as_view(description)
        .contains(
            R"({"type":"record","index":"0x3104","path":"drive.slope",)"
            R"("title":"Speed slope","title_ru":"Темп изменения скорости",)"
            R"("class":"parameters","datatype":"float32","unit":"rpm/s",)"
            R"("layout":1})"
            "\n"));

// The members that show a value inherit the record's type and unit; those
// that apply and discard it are commands of their own type, even in the
// record of a uint32.
static_assert(
    as_view(description)
        .contains(
            R"({"type":"field","layout":2,"sub":"0x01","name":"requested",)"
            R"("title":"Requested value","title_ru":"Запрошенное значение",)"
            R"("access":"ro","inherits":true})"
            "\n"));
static_assert(
    as_view(description)
        .contains(
            R"({"type":"field","layout":2,"sub":"0x08","name":"apply",)"
            R"("title":"Apply","title_ru":"Применить","datatype":"uint32",)"
            R"("access":"wo","command":true})"
            "\n"));
static_assert(
    as_view(description)
        .contains(R"({"type":"field","layout":1,"sub":"0x0A","name":"actual",)"
                  R"("title":"Actual value","title_ru":"Фактическое значение",)"
                  R"("access":"ro","inherits":true})"
                  "\n"));

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
  if (od.read({0x3101, 0x01}) != od_read_result{std::int32_t{11}}) return false;

  // The last refusal of a save shows as well: one while a value waits to be
  // applied answers 08000022h.
  if (!od.write({0x3100, 0x01}, od_value{false})) return false;
  test_applier checker;
  if (c.section.save(checker)) return false;
  return od.read({0x3001, 0x03}) == od_read_result{std::uint32_t{0x08000022}};
}

static_assert(test_a_record_shows_its_parameter());
static_assert(test_a_record_applies_its_parameter());
static_assert(test_commands());

// -- Coverage --

consteval std::string check_missing()
{
  static constexpr od_row<ctx> rows[] = {
      {{0x3100, 0x00}, {}, bridge::param<drive_phase_swap>},
  };
  return detail::od_check(rows, areas, groups);
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
      .layout = bind<bridge::param<prod_serial>>().layout,
      .field_labels = bind<bridge::param<prod_serial>>().field_labels};
  // clang-format off
  static constexpr od_row<ctx> rows[] = {
      {{0x3100, 0x00}, {},                                     bridge::param<drive_phase_swap>},
      {{0x3101, 0x00}, {},                                     bridge::param<motor_p>},
      {{0x3102, 0x00}, {},                                     bridge::param<drive_freq>},
      {{0x3103, 0x00}, {},                                     bridge::param<prod_serial>},
      {{0x3104, 0x00}, {"prod.secret", {"Secret", "Секрет"}}, secret},
      {{0x3105, 0x00}, {},                                     bridge::param<drive_slope>},
  };
  // clang-format on
  return detail::od_check(rows, areas, groups);
}

static_assert(check_hidden()
              == "od: settings parameter 'prod.secret' is not exposed but has "
                 "a row: 3104h:00 prod.secret");

// The record takes its label from the schema; a row that gives one as well is
// reported at the row.
consteval std::string check_label()
{
  // clang-format off
  static constexpr od_row<ctx> rows[] = {
      {{0x3101, 0x00}, {"motor.pole_pairs", {"Pole pairs", "Число пар полюсов"}}, bridge::param<motor_p>},
  };
  // clang-format on
  return detail::od_check(rows, areas, groups);
}

static_assert(check_label()
              == "od: 3101h:00 motor.p: the row has a label, but its binding "
                 "gives one");

} // namespace
