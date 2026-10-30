#include <emb/can/canopen/od.hpp>
#include <emb/can/canopen/od_handlers.hpp>
#include <emb/can/canopen/pdo.hpp>
#include <emb/can/canopen/types.hpp>

#include <array>
#include <bit>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <ratio>
#include <span>
#include <string_view>
#include <utility>

#include <emb/can.hpp>
#include <emb/can/rolling_counter.hpp>
#include <emb/math/clamped.hpp>
#include <emb/math/scaled.hpp>
#include <emb/meta/enum_info.hpp>
#include <emb/units.hpp>

namespace {

using namespace emb::can::canopen;
using emb::can::payload_t;

namespace drive {

enum class state : std::uint32_t {
  init,
  standby,
  powering_up,
  powering_down,
  ready,
  running,
  stopping,
  testing,
  calibrating,
  fault
};

consteval std::span<emb::enumerator_info<state> const> enum_info_of(state)
{
  static constexpr emb::enumerator_info<state> info[] = {
      {state::init, "init", {"Init", "Инициализация"}},
      {state::standby, "standby", {"Standby", "Ожидание"}},
      {state::powering_up, "powering_up", {"Powering up", "Включение"}},
      {state::powering_down, "powering_down", {"Powering down", "Выключение"}},
      {state::ready, "ready", {"Ready", "Готов"}},
      {state::running, "running", {"Running", "Работа"}},
      {state::stopping, "stopping", {"Stopping", "Останов"}},
      {state::testing, "testing", {"Testing", "Тестирование"}},
      {state::calibrating, "calibrating", {"Calibrating", "Калибровка"}},
      {state::fault, "fault", {"Fault", "Авария"}},
  };
  return info;
}

enum class control_mode : std::uint32_t {
  torque,
  speed,
  angle
};

consteval std::span<emb::enumerator_info<control_mode> const>
enum_info_of(control_mode)
{
  static constexpr emb::enumerator_info<control_mode> info[] = {
      {control_mode::torque, "torque", {"Torque", "Момент"}},
      {control_mode::speed, "speed", {"Speed", "Скорость"}},
      {control_mode::angle, "angle", {"Angle", "Угол"}},
  };
  return info;
}

enum class model_mode : std::uint8_t {
  idle,
  closed_loop,
  semiclosed_loop,
  open_current_loop,
  open_voltage_loop,
  direct_pwm
};

consteval std::span<emb::enumerator_info<model_mode> const>
enum_info_of(model_mode)
{
  static constexpr emb::enumerator_info<model_mode> info[] = {
      {model_mode::idle, "idle", {"Idle", "Ожидание"}},
      {model_mode::closed_loop, "closed_loop", {"Closed loop", "Замкнутый"}},
      {model_mode::semiclosed_loop,
       "semiclosed_loop",
       {"Semiclosed loop", "Полузамкнутый"}},
      {model_mode::open_current_loop,
       "open_current_loop",
       {"Open current loop", "Разомкнутый I"}},
      {model_mode::open_voltage_loop,
       "open_voltage_loop",
       {"Open voltage loop", "Разомкнутый V"}},
      {model_mode::direct_pwm, "direct_pwm", {"Direct PWM", "Тест ШИМ"}},
  };
  return info;
}

enum class level : std::uint8_t {
  warning,
  error,
  critical,
  emergency
};

consteval std::span<emb::enumerator_info<level> const> enum_info_of(level)
{
  static constexpr emb::enumerator_info<level> info[] = {
      {level::warning, "warning", {"Warning", "Предупреждение"}},
      {level::error, "error", {"Error", "Ошибка"}},
      {level::critical, "critical", {"Critical", "Критическая ошибка"}},
      {level::emergency, "emergency", {"Emergency", "Авария"}},
  };
  return info;
}

// A signed enumeration with a gap: -1, 0 and 2.
enum class tilt : std::int8_t {
  down = -1,
  level = 0,
  up = 2
};

consteval std::span<emb::enumerator_info<tilt> const> enum_info_of(tilt)
{
  static constexpr emb::enumerator_info<tilt> info[] = {
      {tilt::down, "down", {"Down", "Вниз"}},
      {tilt::level, "level", {"Level", "Ровно"}},
      {tilt::up, "up", {"Up", "Вверх"}},
  };
  return info;
}

} // namespace drive

using decivolt =
    emb::scaled<std::int16_t, emb::units::volt_f32, std::ratio<1, 10>>;
using signed_pu =
    emb::scaled<std::int16_t, emb::signed_pu_f32, std::ratio<1, 10000>>;
using rpm = emb::scaled<std::int16_t, emb::units::rpm_f32, std::ratio<1, 1>>;

// A scaled value made from its raw value: clang does not evaluate std::lround,
// through which a scaled value is made from a quantity, at compile time.
template<typename T>
constexpr T from_raw(typename T::raw_type raw)
{
  return std::bit_cast<T>(raw);
}

// -- the drive's status, as etk's TPDO1 lays it out ---------------------------

struct drive_status {
  drive::state state;
  bool pwm_on;
  bool warning;
  bool error;
  bool critical;
  bool emergency;
  drive::control_mode control_mode;
  drive::model_mode model_mode;
  std::uint8_t counter = 0;
};

// clang-format off
inline constexpr pdo_row<drive_status> drive_status_rows[] = {
{{ 0, 8}, "state", {"Drive state", "Состояние автомата"},              pdo_map<&drive_status::state>},
{{ 8, 1}, "pwm_on", {"PWM on", "ШИМ включён"},                         pdo_map<&drive_status::pwm_on>},
{{ 9, 1}, "warning", {"Warning", "Предупреждение"},                    pdo_map<&drive_status::warning>},
{{10, 1}, "error", {"Error", "Ошибка"},                                pdo_map<&drive_status::error>},
{{11, 1}, "critical", {"Critical fault", "Критическая неисправность"}, pdo_map<&drive_status::critical>},
{{12, 1}, "emergency", {"Emergency", "Аварийный останов"},             pdo_map<&drive_status::emergency>},
{{16, 2}, "control_mode", {"Control mode", "Режим управления"},        pdo_map<&drive_status::control_mode>},
{{18, 3}, "model_mode", {"Model mode", "Режим модели"},                pdo_map<&drive_status::model_mode>},
{{56, 2}, {}, {},                                                      pdo_map_counter<&drive_status::counter>},
};
// clang-format on

static_assert(some_pdo_rows<std::remove_cvref_t<decltype(drive_status_rows)>>);
static_assert(std::same_as<pdo_message_t<drive_status_rows>, drive_status>);
static_assert(detail::pdo_check(drive_status_rows).empty());

// Running, the PWM on, an error, following a speed; the counter, stamped as
// the fourth frame of the stream, is 3.
consteval bool test_encode_status()
{
  emb::can::rolling_counter<2> counter;
  drive_status message{.state = drive::state::running,
                       .pwm_on = true,
                       .warning = false,
                       .error = true,
                       .critical = false,
                       .emergency = false,
                       .control_mode = drive::control_mode::speed,
                       .model_mode = drive::model_mode::idle};
  for (auto k = 0; k < 4; ++k) {
    counter.stamp(message);
  }
  return encode_pdo<drive_status_rows>(message)
      == payload_t{0x05, 0x05, 0x01, 0x00, 0x00, 0x00, 0x00, 0x03};
}

static_assert(test_encode_status());

// Returns the frame of the drive's status in which `set` alone sets a member,
// as a number.
template<typename F>
constexpr std::uint64_t status_word(F set)
{
  drive_status message{};
  set(message);
  return std::bit_cast<std::uint64_t>(encode_pdo<drive_status_rows>(message));
}

// Every signal takes its own bits: each member alone, at its largest value,
// sets exactly the bits of its signal.
static_assert(status_word([](drive_status& m) {
                m.state = drive::state::fault;
              })
              == 0x09);
static_assert(status_word([](drive_status& m) { m.pwm_on = true; })
              == std::uint64_t{1} << 8);
static_assert(status_word([](drive_status& m) { m.warning = true; })
              == std::uint64_t{1} << 9);
static_assert(status_word([](drive_status& m) { m.critical = true; })
              == std::uint64_t{1} << 11);
static_assert(status_word([](drive_status& m) { m.emergency = true; })
              == std::uint64_t{1} << 12);
static_assert(status_word([](drive_status& m) {
                m.control_mode = drive::control_mode::angle;
              })
              == std::uint64_t{2} << 16);
static_assert(status_word([](drive_status& m) {
                m.model_mode = drive::model_mode::direct_pwm;
              })
              == std::uint64_t{5} << 18);
static_assert(status_word([](drive_status& m) { m.counter = 3; })
              == std::uint64_t{3} << 56);

// Each row puts the low bits of its member's raw value into its signal: a
// counter of 6 takes the two bits of its signal, 2, and leaves the bits above
// alone.
static_assert(status_word([](drive_status& m) { m.counter = 6; })
              == std::uint64_t{2} << 56);

// A TPDO decodes as a host would read it: the frame that the inverter sent
// in standby, with a warning and a critical fault, counter 3.
consteval bool test_decode_status()
{
  auto const status = decode_pdo<drive_status_rows>(
      {0x01, 0x0A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03});
  return status
      && status->state == drive::state::standby
      && !status->pwm_on
      && status->warning
      && !status->error
      && status->critical
      && !status->emergency
      && status->control_mode == drive::control_mode::torque
      && status->model_mode == drive::model_mode::idle
      && status->counter == 3;
}

static_assert(test_decode_status());

// -- the drive's command, as etk's RPDO1 lays it out --------------------------

struct drive_command {
  bool emergency;
  bool power;
  bool start;
  drive::control_mode control_mode;
  signed_pu torque_ref;
  rpm speed_ref;
  std::uint8_t counter = 0;
};

// clang-format off
inline constexpr pdo_row<drive_command> drive_command_rows[] = {
{{ 0,  1}, "emergency", {"Emergency stop", "Аварийный останов"},  pdo_map<&drive_command::emergency>},
{{ 1,  1}, "power", {"Power", "Питание"},                         pdo_map<&drive_command::power>},
{{ 2,  1}, "start", {"Start", "Пуск"},                            pdo_map<&drive_command::start>},
{{ 3,  2}, "control_mode", {"Control mode", "Режим управления"},  pdo_map<&drive_command::control_mode>},
{{16, 16}, "torque_ref", {"Torque reference", "Задание момента"}, pdo_map<&drive_command::torque_ref>},
{{32, 16}, "speed_ref", {"Speed reference", "Задание скорости"},  pdo_map<&drive_command::speed_ref>},
{{56,  2}, {}, {},                                                pdo_map_counter<&drive_command::counter>},
};
// clang-format on

static_assert(pdo_reserved_mask<drive_command_rows> == 0xFCFF'0000'0000'FFE0);

inline constexpr payload_t command_frame =
    {0x0E, 0x00, 0xF0, 0xD8, 0xDC, 0x05, 0x00, 0x01};

// Power, start and a speed to follow; a torque of -10000 raw steps, which
// takes the sign from the top of its signal, and a speed of 1500; counter 1.
consteval bool test_decode_command()
{
  auto const command = decode_pdo<drive_command_rows>(command_frame);
  return command
      && !command->emergency
      && command->power
      && command->start
      && command->control_mode == drive::control_mode::speed
      && command->torque_ref.raw() == -10000
      && command->speed_ref.raw() == 1500
      && command->counter == 1;
}

static_assert(test_decode_command());

// A frame decodes and encodes back to itself, and a message encodes and
// decodes back to itself.
static_assert(encode_pdo<drive_command_rows>(
                  *decode_pdo<drive_command_rows>(command_frame))
              == command_frame);

consteval bool test_command_round_trip()
{
  drive_command const command{.emergency = true,
                              .power = false,
                              .start = true,
                              .control_mode = drive::control_mode::angle,
                              .torque_ref = from_raw<signed_pu>(-32768),
                              .speed_ref = from_raw<rpm>(32767),
                              .counter = 2};
  auto const back =
      decode_pdo<drive_command_rows>(encode_pdo<drive_command_rows>(command));
  return back
      && back->emergency
      && !back->power
      && back->start
      && back->control_mode == drive::control_mode::angle
      && back->torque_ref.raw() == -32768
      && back->speed_ref.raw() == 32767
      && back->counter == 2;
}

static_assert(test_command_round_trip());

// A control mode of 3, which its enumeration does not label, refuses the frame
// at its row, the fourth; a reserved bit refuses it before any row is read,
// wherever it lies.
static_assert(
    decode_pdo<drive_command_rows>({0x18, 0, 0, 0, 0, 0, 0, 0}).error()
    == pdo_decode_failure{.reason = pdo_decode_refusal::unnamed_value,
                          .row = 3});
static_assert(decode_pdo<drive_command_rows>({0x00, 0x01, 0, 0, 0, 0, 0, 0})
                  .error()
                  .reason
              == pdo_decode_refusal::reserved_bit);
static_assert(decode_pdo<drive_command_rows>({0x18, 0x01, 0, 0, 0, 0, 0, 0})
                  .error()
                  .reason
              == pdo_decode_refusal::reserved_bit);
static_assert(
    decode_pdo<drive_command_rows>({0x20, 0, 0, 0, 0, 0, 0, 0}).error().reason
    == pdo_decode_refusal::reserved_bit);
static_assert(
    decode_pdo<drive_command_rows>({0, 0, 0, 0, 0, 0, 0x80, 0}).error().reason
    == pdo_decode_refusal::reserved_bit);
static_assert(
    decode_pdo<drive_command_rows>({0, 0, 0, 0, 0, 0, 0, 0x04}).error().reason
    == pdo_decode_refusal::reserved_bit);

// `peek_pdo` reads a member of a frame that `decode_pdo` refuses: the
// emergency bit and the counter of a frame with an unnamed control mode, and
// the counter of one with reserved bits set.
inline constexpr payload_t refused_frame = {0x19, 0, 0, 0, 0, 0, 0, 0x02};
static_assert(!decode_pdo<drive_command_rows>(refused_frame));
static_assert(
    peek_pdo<&drive_command::emergency, drive_command_rows>(refused_frame));
static_assert(
    peek_pdo<&drive_command::counter, drive_command_rows>(refused_frame) == 2);
static_assert(peek_pdo<&drive_command::counter, drive_command_rows>(
                  {0, 0, 0, 0, 0, 0, 0xFC, 0x07})
              == 3);
static_assert(
    !peek_pdo<&drive_command::emergency, drive_command_rows>(command_frame));
static_assert(
    peek_pdo<&drive_command::control_mode, drive_command_rows>(refused_frame)
    == static_cast<drive::control_mode>(3));
static_assert(
    peek_pdo<&drive_command::torque_ref, drive_command_rows>(command_frame)
        .raw()
    == -10000);

// -- the status flags of one level, as etk's TPDO4 lays them out --------------

struct status_flags {
  std::uint64_t flags;
  drive::level level;
  std::uint8_t counter = 0;
};

// clang-format off
inline constexpr pdo_row<status_flags> status_flags_rows[] = {
{{ 0, 39}, "flags", {"Status flags", "Флаги статусов"}, pdo_map<&status_flags::flags>},
{{48,  2}, "level", {"Level", "Уровень"},               pdo_map<&status_flags::level>},
{{56,  2}, {}, {},                                      pdo_map_counter<&status_flags::counter>},
};
// clang-format on

// A signal of 39 bits in a member of 64: the flags of statuses 14 and 38.
static_assert(encode_pdo<status_flags_rows>({.flags = (std::uint64_t{1} << 38)
                                                    | 0x4000,
                                             .level = drive::level::warning,
                                             .counter = 2})
              == payload_t{0x00, 0x40, 0x00, 0x00, 0x40, 0x00, 0x00, 0x02});
static_assert(decode_pdo<status_flags_rows>(
                  {0x00, 0x40, 0x00, 0x00, 0x40, 0x00, 0x03, 0x02})
                  ->flags
              == ((std::uint64_t{1} << 38) | 0x4000));
static_assert(decode_pdo<status_flags_rows>({0, 0, 0, 0, 0, 0, 0x03, 0})->level
              == drive::level::emergency);
// Bit 39, past the signal, is reserved.
static_assert(
    decode_pdo<status_flags_rows>({0, 0, 0, 0, 0x80, 0, 0, 0}).error().reason
    == pdo_decode_refusal::reserved_bit);

// -- the elements of an array, in rows of an std::array -----------------------

struct page {
  std::array<std::uint8_t, 4> severities{};
  std::uint8_t counter = 0;
};

inline constexpr std::string_view slot_names[] = {"slot0",
                                                  "slot1",
                                                  "slot2",
                                                  "slot3"};

template<std::size_t... I>
consteval std::array<pdo_row<page>, sizeof...(I) + 1>
page_rows_of(std::index_sequence<I...>)
{
  return {{{{static_cast<std::uint8_t>(3 * I), 3},
            slot_names[I],
            {"Slot", "Слот"},
            pdo_map_element<&page::severities, I>}...,
           {{56, 2}, {}, {}, pdo_map_counter<&page::counter>}}};
}

inline constexpr auto page_rows = page_rows_of(std::make_index_sequence<4>{});

static_assert(some_pdo_rows<std::remove_cvref_t<decltype(page_rows)>>);
static_assert(std::same_as<pdo_message_t<page_rows>, page>);

// Slot 2 takes bits 6 to 8: a value of 5 there is 40h in byte 0 and 1 in
// byte 1.
consteval bool test_page()
{
  page message{};
  message.severities[2] = 5;
  message.counter = 1;
  auto const payload = encode_pdo<page_rows>(message);
  auto const back = decode_pdo<page_rows>(payload);
  return payload == payload_t{0x40, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01}
      && back
      && back->severities[2] == 5
      && back->severities[1] == 0
      && back->severities[3] == 0
      && back->counter == 1;
}

static_assert(test_page());

// Slot 3 takes bits 9 to 11; bit 12 is reserved.
static_assert(decode_pdo<page_rows>({0, 0x0E, 0, 0, 0, 0, 0, 0})->severities[3]
              == 7);
static_assert(decode_pdo<page_rows>({0, 0x10, 0, 0, 0, 0, 0, 0}).error().reason
              == pdo_decode_refusal::reserved_bit);

// Each element has a getter of its own, so that no rule sees two rows map one
// member.
static_assert(pdo_map_element<&page::severities, 0>.get
              != pdo_map_element<&page::severities, 1>.get);

// -- a signed enumeration with a gap ------------------------------------------

struct attitude {
  drive::tilt tilt;
};

inline constexpr pdo_row<attitude> attitude_rows[] = {
    {{0, 3}, "tilt", {"Tilt", "Наклон"}, pdo_map<&attitude::tilt>},
};

// -1 takes the sign from the top of the signal; 1, in the gap, and -4 are no
// values of the enumeration.
static_assert(encode_pdo<attitude_rows>({.tilt = drive::tilt::down})[0]
              == 0x07);
static_assert(decode_pdo<attitude_rows>({0x07, 0, 0, 0, 0, 0, 0, 0})->tilt
              == drive::tilt::down);
static_assert(decode_pdo<attitude_rows>({0x02, 0, 0, 0, 0, 0, 0, 0})->tilt
              == drive::tilt::up);
static_assert(decode_pdo<attitude_rows>({0x01, 0, 0, 0, 0, 0, 0, 0}).error()
              == pdo_decode_failure{.reason = pdo_decode_refusal::unnamed_value,
                                    .row = 0});
static_assert(decode_pdo<attitude_rows>({0x04, 0, 0, 0, 0, 0, 0, 0}).error()
              == pdo_decode_failure{.reason = pdo_decode_refusal::unnamed_value,
                                    .row = 0});
static_assert(
    peek_pdo<&attitude::tilt, attitude_rows>({0x04, 0, 0, 0, 0, 0, 0, 0})
    == static_cast<drive::tilt>(-4));

// -- what the description says of a member -----------------------------------

struct electrical_status {
  decivolt dc_voltage;
  std::uint8_t counter = 0;
};

// A scaled member gives the unit of its quantity and its step, and its signal
// reads as its raw type does; an enumeration gives the table of its
// enumerators, the one that the dictionary gives an object of it, and the
// lowest and highest values named; a bool and an integer give neither unit nor
// table.
inline constexpr auto dc_voltage = pdo_map<&electrical_status::dc_voltage>;
static_assert(dc_voltage.role == pdo_role::value);
static_assert(dc_voltage.unit == emb::i18n::text{"V", "В"});
static_assert(dc_voltage.scale_num == 1 && dc_voltage.scale_den == 10);
static_assert(dc_voltage.value_table.empty());
static_assert(dc_voltage.encoding == pdo_encoding::signed_);
static_assert(dc_voltage.max_width == 16);

inline constexpr auto torque_ref = pdo_map<&drive_command::torque_ref>;
static_assert(torque_ref.unit == emb::i18n::text{"pu", "о. е."});
static_assert(torque_ref.scale_num == 1 && torque_ref.scale_den == 10000);

inline constexpr auto speed_ref = pdo_map<&drive_command::speed_ref>;
static_assert(speed_ref.unit == emb::i18n::text{"rpm", "об/мин"});
static_assert(speed_ref.scale_num == 1 && speed_ref.scale_den == 1);

inline constexpr auto state = pdo_map<&drive_status::state>;
static_assert(state.unit.empty());
static_assert(state.scale_num == 1 && state.scale_den == 1);
static_assert(state.value_table.data()
              == detail::od_value_table_of<drive::state>.data());
static_assert(state.value_table.size() == 10);
static_assert(state.value_table[9].name == "fault");
static_assert(state.encoding == pdo_encoding::unsigned_);
static_assert(state.max_width == 32);
static_assert(state.lowest_named == 0 && state.highest_named == 9);

inline constexpr auto tilt = pdo_map<&attitude::tilt>;
static_assert(tilt.encoding == pdo_encoding::signed_);
static_assert(tilt.max_width == 8);
static_assert(tilt.lowest_named == -1 && tilt.highest_named == 2);

inline constexpr auto pwm_on = pdo_map<&drive_status::pwm_on>;
static_assert(pwm_on.unit.empty() && pwm_on.value_table.empty());
static_assert(pwm_on.scale_num == 1 && pwm_on.scale_den == 1);
static_assert(pwm_on.max_width == 1 && pwm_on.takes == nullptr);

inline constexpr auto flags = pdo_map<&status_flags::flags>;
static_assert(flags.unit.empty() && flags.value_table.empty());
static_assert(flags.encoding == pdo_encoding::unsigned_);
static_assert(flags.max_width == 64 && flags.takes == nullptr);

// A counter is its member's codec with the role `counter`: the same getter,
// which `peek_pdo` finds it by.
inline constexpr auto counter = pdo_map_counter<&electrical_status::counter>;
static_assert(counter.role == pdo_role::counter);
static_assert(counter.get == pdo_map<&electrical_status::counter>.get);
static_assert(counter.max_width == 8);
static_assert(counter.unit.empty() && counter.value_table.empty());

// -- the rules of the rows of a PDO -------------------------------------------

struct members {
  bool flag;
  drive::control_mode mode;
  drive::tilt tilt;
  std::uint8_t number;
  std::uint64_t wide;
  std::uint16_t wide_counter = 0;
  std::uint8_t counter = 0;
  std::uint8_t other_counter = 0;
};

inline constexpr std::string_view any_name = "any";
inline constexpr emb::i18n::text any_label = {"Any", "Любой"};

template<auto const& Rows>
constexpr bool rule_is(std::string_view text)
{
  return detail::pdo_check(Rows) == text;
}

inline constexpr pdo_row<members> all_rules_kept[] = {
    {{0, 1}, any_name, any_label, pdo_map<&members::flag>},
    {{1, 2}, any_name, any_label, pdo_map<&members::mode>},
    {{3, 3}, any_name, any_label, pdo_map<&members::tilt>},
    {{8, 8}, any_name, any_label, pdo_map<&members::number>},
    {{16, 32}, any_name, any_label, pdo_map<&members::wide>},
    {{48, 8}, {}, {}, pdo_map_counter<&members::wide_counter>},
};
static_assert(rule_is<all_rules_kept>(""));

inline constexpr pdo_row<members> no_bits[] = {
    {{3, 0}, any_name, any_label, pdo_map<&members::number>},
};
static_assert(rule_is<no_bits>(
    "pdo: the signal at bit 3 must be from 1 to the bits of its member's "
    "type"));

inline constexpr pdo_row<members> wider_than_type[] = {
    {{3, 9}, any_name, any_label, pdo_map<&members::number>},
};
static_assert(rule_is<wider_than_type>(
    "pdo: the signal at bit 3 must be from 1 to the bits of its member's "
    "type"));

// The rules of the bits come before those of the name and the label.
inline constexpr pdo_row<members> wide_bool[] = {
    {{3, 2}, {}, {}, pdo_map<&members::flag>},
};
static_assert(rule_is<wide_bool>(
    "pdo: the signal at bit 3 must be from 1 to the bits of its member's "
    "type"));

inline constexpr pdo_row<members> past_the_frame[] = {
    {{60, 8}, any_name, any_label, pdo_map<&members::number>},
};
static_assert(
    rule_is<past_the_frame>("pdo: the signal at bit 60 must lie within the 64 "
                            "bits"));

inline constexpr pdo_row<members> past_bit_99[] = {
    {{100, 8}, any_name, any_label, pdo_map<&members::number>},
};
static_assert(
    rule_is<past_bit_99>("pdo: the signal at bit 100 must lie within the 64 "
                         "bits"));

inline constexpr pdo_row<members> whole_frame[] = {
    {{0, 64}, any_name, any_label, pdo_map<&members::wide>},
};
static_assert(rule_is<whole_frame>(""));

inline constexpr pdo_row<members> narrow_enumeration[] = {
    {{3, 1}, any_name, any_label, pdo_map<&members::mode>},
};
static_assert(rule_is<narrow_enumeration>(
    "pdo: the signal at bit 3 is too narrow for the named values"));

// A signed signal of 2 bits holds -2 to 1, and 2, the highest value named,
// does not fit it; one of 3 bits holds it.
inline constexpr pdo_row<members> narrow_signed[] = {
    {{3, 2}, any_name, any_label, pdo_map<&members::tilt>},
};
static_assert(rule_is<narrow_signed>(
    "pdo: the signal at bit 3 is too narrow for the named values"));

inline constexpr pdo_row<members> overlap[] = {
    {{0, 2}, any_name, any_label, pdo_map<&members::number>},
    {{1, 1}, any_name, any_label, pdo_map<&members::flag>},
};
static_assert(
    rule_is<overlap>("pdo: the signal at bit 1 overlaps the signal at bit 0"));

inline constexpr pdo_row<members> overlap_later[] = {
    {{8, 8}, any_name, any_label, pdo_map<&members::number>},
    {{0, 9}, any_name, any_label, pdo_map<&members::wide>},
};
static_assert(rule_is<overlap_later>(
    "pdo: the signal at bit 0 overlaps the signal at bit 8"));

inline constexpr pdo_row<members> member_twice[] = {
    {{0, 1}, any_name, any_label, pdo_map<&members::flag>},
    {{1, 1}, any_name, any_label, pdo_map<&members::flag>},
};
static_assert(rule_is<member_twice>(
    "pdo: the signal at bit 1 maps the member of the signal at bit 0"));

// A counter has the getter of its member.
inline constexpr pdo_row<members> counter_twice[] = {
    {{0, 2}, any_name, any_label, pdo_map<&members::counter>},
    {{56, 2}, {}, {}, pdo_map_counter<&members::counter>},
};
static_assert(rule_is<counter_twice>(
    "pdo: the signal at bit 56 maps the member of the signal at bit 0"));

inline constexpr pdo_row<members> two_counters[] = {
    {{0, 2}, {}, {}, pdo_map_counter<&members::counter>},
    {{56, 2}, {}, {}, pdo_map_counter<&members::other_counter>},
};
static_assert(rule_is<two_counters>("pdo: a PDO has at most one counter"));

inline constexpr pdo_row<members> wide_counter[] = {
    {{48, 9}, {}, {}, pdo_map_counter<&members::wide_counter>},
};
static_assert(rule_is<wide_counter>(
    "pdo: the signal at bit 48 is a counter and takes one to eight bits"));

// A row has no unit to give: a signal takes its unit from the type of its
// member alone, while a unit given beside the label would replace that of the
// type, as the info of an object of the dictionary does, and "%" would show a
// torque of half the rated one as 0.5 %.
template<typename Info>
concept info_takes_unit = requires {
  Info{"torque_ref", {"Torque reference", "Задание момента"}, {"%", "%"}};
};
template<typename Row>
concept row_takes_unit = requires {
  Row{{0, 8},
      "torque_ref",
      {"Torque reference", "Задание момента"},
      {"%", "%"}};
};
static_assert(info_takes_unit<od_object_info>
              && !row_takes_unit<pdo_row<members>>);

inline constexpr pdo_row<members> counter_with_name[] = {
    {{56, 2}, "counter", {}, pdo_map_counter<&members::counter>},
};
static_assert(rule_is<counter_with_name>(
    "pdo: the signal at bit 56 is a counter and has no name or label"));

inline constexpr pdo_row<members> counter_with_label[] = {
    {{56, 2}, "", {"Counter", ""}, pdo_map_counter<&members::counter>},
};
static_assert(rule_is<counter_with_label>(
    "pdo: the signal at bit 56 is a counter and has no name or label"));

inline constexpr pdo_row<members> no_name[] = {
    {{8, 8}, "", {"Number", "Число"}, pdo_map<&members::number>},
};
static_assert(
    rule_is<no_name>("pdo: the signal at bit 8 needs a name and a label"));

inline constexpr pdo_row<members> no_russian_label[] = {
    {{8, 8}, "number", {"Number", ""}, pdo_map<&members::number>},
};
static_assert(rule_is<no_russian_label>(
    "pdo: the signal at bit 8 needs a name and a label"));

} // namespace
