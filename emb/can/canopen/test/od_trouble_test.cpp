#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string_view>

#include <emb/can/canopen/od_description.hpp>
#include <emb/can/canopen/od_dictionary.hpp>
#include <emb/can/canopen/od_handlers.hpp>
#include <emb/can/canopen/od_trouble.hpp>
#include <emb/i18n.hpp>
#include <emb/meta/enum_info.hpp>
#include <emb/meta/typelist.hpp>
#include <emb/trouble.hpp>

namespace {

using namespace emb::can::canopen;

// The levels of the statuses, labelled for a host.
enum class level : std::uint8_t { warning, fault };

consteval std::span<emb::enumerator_info<level> const> enum_info(level)
{
  static constexpr emb::enumerator_info<level> info[] = {
      {level::warning, "warning", {"Warning", "Предупреждение"}},
      {level::fault, "fault", {"Fault", "Отказ"}},
  };
  return info;
}

struct overheat {
  static constexpr emb::trouble::id_type id = 0;
  static constexpr level level_min = level::warning;
  static constexpr level level_max = level::fault;
  static constexpr std::string_view name = "overheat";
  static constexpr emb::i18n title = {"Overheat", "Перегрев"};
};

struct overvoltage {
  static constexpr emb::trouble::id_type id = 1;
  static constexpr level level_min = level::fault;
  static constexpr level level_max = level::fault;
  static constexpr std::string_view name = "overvoltage";
  static constexpr emb::i18n title = {"Overvoltage", "Перенапряжение"};
};

// The status of the id 2 is retired.

struct stall {
  static constexpr emb::trouble::id_type id = 3;
  static constexpr level level_min = level::warning;
  static constexpr level level_max = level::fault;
  static constexpr std::string_view name = "stall";
  static constexpr emb::i18n title = {"Stall", "Заклинивание"};
};

using statuses = emb::typelist<overheat, overvoltage, stall>;

// A registry whose severities are constants, in place of emb::trouble's,
// whose words are atomics that constant evaluation does not read: the bridge
// calls only `severity`.
struct fixed_registry {
  std::array<std::optional<level>, 4> severities{};

  template<typename Status>
  constexpr std::optional<level> severity(Status) const
  {
    return severities[Status::id];
  }
};

inline constexpr fixed_registry registry{
    {level::fault, std::nullopt, std::nullopt, level::warning}};

} // namespace

// The bridge takes the statuses and the levels of a registry from its type,
// which names them for emb::trouble::registry and here for the registry in
// its place.
template<>
struct emb::can::canopen::detail::od_registry_traits<fixed_registry> {
  using status_list = statuses;
  using level_type = level;
  static constexpr std::size_t level_count = 2;
};

namespace {

// -- Severity --

// 0 for no level, and the level + 1 for a level.
static_assert(severity_value_of<level>(std::nullopt) == 0);
static_assert(severity_value_of(std::optional{level::warning}) == 1);
static_assert(severity_value_of(std::optional{level::fault}) == 2);

// The scale names no level first, then each level at its severity.
static_assert(od_severity_labels<level>.size() == 3);
static_assert(od_severity_labels<level>[0].value == 0);
static_assert(od_severity_labels<level>[0].name == "none");
static_assert(od_severity_labels<level>[0].title == emb::i18n{"None", "Нет"});
static_assert(od_severity_labels<level>[1].value == 1);
static_assert(od_severity_labels<level>[1].name == "warning");
static_assert(od_severity_labels<level>[2].value == 2);
static_assert(od_severity_labels<level>[2].name == "fault");
static_assert(od_severity_labels<level>[2].title
              == emb::i18n{"Fault", "Отказ"});

// The stub of a retired status is named after its id, in decimal digits.
static_assert(detail::od_decimal<0>.view() == "0");
static_assert(detail::od_decimal<10>.view() == "10");
static_assert(detail::od_retired_name<22>.view() == "retired_22");
static_assert(detail::od_retired_title_en<253>.view() == "Retired status 253");
static_assert(detail::od_retired_title_ru<7>.view() == "Снятый статус 7");

// -- Binding --

struct ctx {};

template<auto Binding>
consteval od_binding<ctx> bind()
{
  return Binding;
}

// A member a subindex, 00h and one for each id, the retired one included;
// the row gives the label, and the record has no value of its own.
static_assert(bind<od_statuses<registry>>().layout.size() == 5);
static_assert(bind<od_statuses<registry>>().field_labels.size() == 5);
static_assert(bind<od_statuses<registry>>().layout[0].access
              == od_access::const_);
static_assert(bind<od_statuses<registry>>().layout[3].access == od_access::ro);
static_assert(bind<od_statuses<registry>>().layout[4].type
              == od_data_type::uint8);
static_assert(bind<od_statuses<registry>>().layout[4].write == nullptr);
static_assert(bind<od_statuses<registry>>().label.path.empty());
static_assert(bind<od_statuses<registry>>().value_labels.empty());

// The member of a status bears its name and title, that of a retired one
// those of the stub, and each past 00h the scale.
static_assert(bind<od_statuses<registry>>().field_labels[0].name
              == "highest_subindex");
static_assert(
    bind<od_statuses<registry>>().field_labels[0].value_labels.empty());
static_assert(bind<od_statuses<registry>>().field_labels[1].name == "overheat");
static_assert(bind<od_statuses<registry>>().field_labels[1].title
              == overheat::title);
static_assert(bind<od_statuses<registry>>().field_labels[3].name
              == "retired_2");
static_assert(bind<od_statuses<registry>>().field_labels[3].title
              == emb::i18n{"Retired status 2", "Снятый статус 2"});
static_assert(bind<od_statuses<registry>>().field_labels[3].value_labels.data()
              == od_severity_labels<level>.data());
static_assert(bind<od_statuses<registry>>().field_labels[4].name == "stall");

// The trait names the statuses and the levels of emb::trouble::registry, and
// the bridge binds one.
using trouble_registry = emb::trouble::registry<statuses, level, 2>;

static_assert(
    std::same_as<detail::od_registry_traits<trouble_registry>::status_list,
                 statuses>);
static_assert(
    std::same_as<detail::od_registry_traits<trouble_registry>::level_type,
                 level>);
static_assert(detail::od_registry_traits<trouble_registry>::level_count == 2);

constinit trouble_registry raised{};

static_assert(bind<od_statuses<raised>>().layout.size() == 5);
static_assert(bind<od_statuses<raised>>().field_labels[4].name == "stall");

// -- The list of an MPDO --

consteval bool is_block(mpdo_scan const& scan,
                        std::uint16_t index,
                        std::uint8_t subindex,
                        std::uint8_t count)
{
  return scan.index == index
      && scan.subindex == subindex
      && scan.count == count;
}

// A block for each run of live statuses, by id, at their subindices: two
// around the retired status 2, one without a hole, and none for 00h or for a
// hole at id 0.
static_assert(od_statuses_scans<registry, 0x4000>.size() == 2);
static_assert(is_block(od_statuses_scans<registry, 0x4000>[0], 0x4000, 1, 2));
static_assert(is_block(od_statuses_scans<registry, 0x4000>[1], 0x4000, 4, 1));
static_assert(od_statuses_scans<raised, 0x4100>.size() == 2);
static_assert(is_block(od_statuses_scans<raised, 0x4100>[1], 0x4100, 4, 1));

constinit emb::trouble::registry<emb::typelist<overheat, overvoltage>, level, 2>
    unbroken{};
constinit emb::trouble::registry<emb::typelist<overvoltage, stall>, level, 2>
    late{};

static_assert(od_statuses_scans<unbroken, 0x4000>.size() == 1);
static_assert(is_block(od_statuses_scans<unbroken, 0x4000>[0], 0x4000, 1, 2));
static_assert(od_statuses_scans<late, 0x4000>.size() == 2);
static_assert(is_block(od_statuses_scans<late, 0x4000>[0], 0x4000, 2, 1));
static_assert(is_block(od_statuses_scans<late, 0x4000>[1], 0x4000, 4, 1));

// -- Dictionary --

enum class mode : std::uint8_t { idle, run };

consteval std::span<emb::enumerator_info<mode> const> enum_info(mode)
{
  static constexpr emb::enumerator_info<mode> info[] = {
      {mode::idle, "idle", {"Idle", "Ожидание"}},
      {mode::run, "run", {"Run", "Работа"}},
  };
  return info;
}

constexpr mode read_mode()
{
  return mode::run;
}

inline constexpr od_area areas[] = {
    {0x4000, 0x4FFF, od_class::status},
    {0x5000, 0x5FFF, od_class::watch},
};

inline constexpr od_group groups[] = {
    {"trouble", {"Trouble", "Неисправности"}},
    {"drive", {"Drive", "Привод"}},
};

// clang-format off
inline constexpr od_row<ctx> rows[] = {
    {{0x5000, 0x01}, {"drive.mode", {"Mode", "Режим"}},                                     od_ro<read_mode>},
    {{0x4000, 0x00}, {"trouble.statuses", {"Trouble statuses", "Статусы неисправностей"}}, od_statuses<registry>},
};
// clang-format on

inline constexpr auto dictionary = make_dictionary<rows, areas, groups>();

// Member id + 1 reads the severity of the status of the id, 00h the highest
// subindex, and the stub of the retired status 0.
consteval bool test_members_read_the_severities()
{
  ctx c;
  od_view<ctx> const view{dictionary};
  auto const read = [&](std::uint8_t subindex) -> od_read_result {
    auto const member = view.find({0x4000, subindex});
    if (!member) return std::unexpected(member.error());
    return member->read(c, member->arg);
  };

  if (read(0x00) != od_read_result{std::uint8_t{4}}) return false;
  if (read(0x01) != od_read_result{std::uint8_t{2}}) return false;
  if (read(0x02) != od_read_result{std::uint8_t{0}}) return false;
  if (read(0x03) != od_read_result{std::uint8_t{0}}) return false;
  if (read(0x04) != od_read_result{std::uint8_t{1}}) return false;
  if (read(0x05) != std::unexpected(sdo_abort_code::subindex_not_found)) {
    return false;
  }

  auto const status = view.find({0x4000, 0x04});
  return status
      && (status->access == od_access::ro)
      && (status->type == od_data_type::uint8)
      && (status->write == nullptr);
}

static_assert(test_members_read_the_severities());

// -- Description --

inline constexpr auto description =
    make_description<rows, areas, groups, "Test device">();

consteval std::string_view as_view(auto const& chars)
{
  return {chars.data(), chars.size()};
}

static_assert(od_description_fingerprint(description)
              == dictionary.fingerprint());

// The record of the class status, of no value of its own: no datatype, unit
// or table. The scale is the table of its members, numbered before the table
// of a row; the members state their type, and the stub is one of them.
inline constexpr std::string_view body =
    R"({"type":"group","name":"trouble","title":"Trouble","title_ru":"Неисправности"})"
    "\n"
    R"({"type":"group","name":"drive","title":"Drive","title_ru":"Привод"})"
    "\n"
    R"({"type":"value_label","value_table":0,"value":0,"name":"none","title":"None","title_ru":"Нет"})"
    "\n"
    R"({"type":"value_label","value_table":0,"value":1,"name":"warning","title":"Warning","title_ru":"Предупреждение"})"
    "\n"
    R"({"type":"value_label","value_table":0,"value":2,"name":"fault","title":"Fault","title_ru":"Отказ"})"
    "\n"
    R"({"type":"value_label","value_table":1,"value":0,"name":"idle","title":"Idle","title_ru":"Ожидание"})"
    "\n"
    R"({"type":"value_label","value_table":1,"value":1,"name":"run","title":"Run","title_ru":"Работа"})"
    "\n"
    R"({"type":"field","layout":0,"sub":"0x00","name":"highest_subindex","title":"Highest subindex","title_ru":"Наибольший подындекс","datatype":"uint8","access":"const"})"
    "\n"
    R"({"type":"field","layout":0,"sub":"0x01","name":"overheat","title":"Overheat","title_ru":"Перегрев","datatype":"uint8","access":"ro","value_table":0})"
    "\n"
    R"({"type":"field","layout":0,"sub":"0x02","name":"overvoltage","title":"Overvoltage","title_ru":"Перенапряжение","datatype":"uint8","access":"ro","value_table":0})"
    "\n"
    R"({"type":"field","layout":0,"sub":"0x03","name":"retired_2","title":"Retired status 2","title_ru":"Снятый статус 2","datatype":"uint8","access":"ro","value_table":0})"
    "\n"
    R"({"type":"field","layout":0,"sub":"0x04","name":"stall","title":"Stall","title_ru":"Заклинивание","datatype":"uint8","access":"ro","value_table":0})"
    "\n"
    R"({"type":"record","index":"0x4000","group":"trouble","name":"statuses","title":"Trouble statuses","title_ru":"Статусы неисправностей","class":"status","layout":0})"
    "\n"
    R"({"type":"object","index":"0x5000","sub":"0x01","group":"drive","name":"mode","title":"Mode","title_ru":"Режим","class":"watch","datatype":"uint32","access":"ro","value_table":1})"
    "\n";

static_assert(as_view(description).ends_with(body));
static_assert(as_view(description).find('\n') + 1 + body.size()
              == description.size());

} // namespace
