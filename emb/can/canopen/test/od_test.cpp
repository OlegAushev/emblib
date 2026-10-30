#include <array>
#include <chrono>
#include <cstdint>
#include <expected>
#include <optional>
#include <ratio>
#include <span>
#include <string>
#include <string_view>
#include <utility>

#include <emb/can/canopen/od.hpp>
#include <emb/can/canopen/od_description.hpp>
#include <emb/can/canopen/od_dictionary.hpp>
#include <emb/can/canopen/od_handlers.hpp>
#include <emb/can/canopen/od_process_data.hpp>
#include <emb/can/canopen/pdo.hpp>
#include <emb/can/canopen/types.hpp>
#include <emb/math/scaled.hpp>
#include <emb/meta/enum_info.hpp>
#include <emb/units.hpp>

namespace {

using namespace emb::can::canopen;
using enum od_data_type;

struct ctx {
  float value = 0.f;
  std::uint32_t commands = 0;
  bool fail = false;
};

constexpr float read_value(ctx const& c)
{
  return c.value;
}

constexpr emb::units::hz_f32 read_freq(ctx const& c)
{
  return emb::units::hz_f32{c.value};
}

// A wrapper with no value() member: value_of alone makes it one.
struct public_field {
  using value_type = float;
  float v;
  constexpr explicit public_field(float x) : v(x) {}
  friend constexpr float value_of(public_field w)
  {
    return w.v;
  }
};

constexpr public_field read_public_field(ctx const& c)
{
  return public_field{c.value};
}

constexpr std::expected<float, sdo_abort_code> read_checked(ctx const& c)
{
  if (c.fail) return std::unexpected(sdo_abort_code::object_not_found);
  return c.value;
}

constexpr std::uint8_t read_u8() noexcept
{
  return 200;
}

constexpr void write_value(ctx& c, float v)
{
  c.value = v;
}

constexpr void write_freq(ctx& c, emb::units::hz_f32 const& v)
{
  c.value = v.value;
}

constexpr od_write_result write_checked(ctx& c, std::int16_t v)
{
  if (v < 0) return std::unexpected(sdo_abort_code::value_too_low);
  c.value = static_cast<float>(v);
  return {};
}

constexpr void command(ctx& c)
{
  ++c.commands;
}

constexpr od_write_result refuse(ctx const&)
{
  return std::unexpected(sdo_abort_code::state_error);
}

constexpr std::string_view text()
{
  return "abcdefg";
}

constexpr char const* word()
{
  return "abcd";
}

constexpr std::array<float, 3> samples()
{
  return {1.5f, 2.5f, 3.5f};
}

inline constexpr std::array rates{emb::units::hz_f32{50.f},
                                  emb::units::hz_f32{60.f}};

// An array of wrapped scalars, by const reference.
constexpr std::array<emb::units::hz_f32, 2> const& read_rates(ctx const&)
{
  return rates;
}

constexpr std::array<float, od_array_capacity> read_longest()
{
  return {};
}

constexpr od_read_result raw_read(ctx& c, std::uint16_t)
{
  return od_value{c.value};
}

constexpr od_write_result raw_write(ctx& c, std::uint16_t, od_value v)
{
  c.value = std::get<float>(v);
  return {};
}

constexpr od_read_result read_arg(ctx&, std::uint16_t arg)
{
  return od_value{arg};
}

constexpr od_read_result read_last_subindex(ctx&, std::uint16_t)
{
  return od_value{std::uint8_t{2}};
}

// The members of a record past 00h: a value kept in the context and the
// record's argument.
inline constexpr od_field<ctx> record_layout[] = {
    {.access = od_access::const_, .type = uint8, .read = &read_last_subindex},
    {.access = od_access::rw,
     .type = float32,
     .read = &raw_read,
     .write = &raw_write},
    {.access = od_access::ro, .type = uint16, .read = &read_arg},
};

// The info of the members of `record_layout`; the value is the record's.
inline constexpr od_field_info record_field_info[] = {
    {.name = "highest_subindex",
     .label = {"Highest subindex", "Наибольший подындекс"}},
    {.name = "value", .label = {"Value", "Значение"}, .inherits = true},
    {.name = "arg", .label = {"Argument", "Аргумент"}},
};

consteval od_binding<ctx> record(std::uint16_t arg)
{
  return {.access = od_access::ro,
          .type = float32,
          .arg = arg,
          .layout = record_layout,
          .field_info = record_field_info};
}

template<typename T>
constexpr T zero()
{
  return T{};
}

struct opaque {};

constexpr double read_double()
{
  return 0.0;
}

constexpr std::int64_t read_i64()
{
  return 0;
}

constexpr opaque read_opaque()
{
  return {};
}

constexpr std::expected<float, std::uint32_t> read_other_error()
{
  return 0.f;
}

constexpr float read_with_arg(ctx&, std::uint16_t)
{
  return 0.f;
}

constexpr float read_by_value(ctx)
{
  return 0.f;
}

constexpr void write_by_reference(ctx&, float&) {}

constexpr float write_returning(ctx&, float)
{
  return 0.f;
}

constexpr void write_nothing(ctx&) {}

constexpr float command_returning(ctx&)
{
  return 0.f;
}

constexpr std::array<double, 2> read_doubles()
{
  return {};
}

constexpr std::array<float, od_array_capacity + 1> read_too_long()
{
  return {};
}

std::array<float, 2> unguarded{};

std::array<float, 2>& read_unguarded()
{
  return unguarded;
}

// -- Types --

static_assert(od_type_of<bool> == od_data_type::boolean);
static_assert(od_type_of<std::int8_t> == od_data_type::int8);
static_assert(od_type_of<std::int16_t> == od_data_type::int16);
static_assert(od_type_of<std::int32_t> == od_data_type::int32);
static_assert(od_type_of<std::uint8_t> == od_data_type::uint8);
static_assert(od_type_of<std::uint16_t> == od_data_type::uint16);
static_assert(od_type_of<std::uint32_t> == od_data_type::uint32);
static_assert(od_type_of<float> == od_data_type::float32);


static_assert(od_readable(od_access::ro)
              && od_readable(od_access::const_)
              && od_readable(od_access::rw)
              && !od_readable(od_access::wo));
static_assert(od_writable(od_access::rw)
              && od_writable(od_access::wo)
              && !od_writable(od_access::ro)
              && !od_writable(od_access::const_));

static_assert(od_key{0x1000, 0x02} < od_key{0x1001, 0x01});
static_assert(od_key{0x1001, 0x01} < od_key{0x1001, 0x02});
static_assert(od_key{0x1001, 0x01} == od_key{0x1001, 0x01});

// The index, the subindex, the type, the access and the argument share the
// first eight bytes; the index and the argument of a record fill its first
// word.
static_assert(sizeof(od_entry<ctx>) == 8 + 2 * sizeof(void*));
static_assert(sizeof(od_record<ctx>) == 3 * sizeof(void*));

// -- Handler shapes --

static_assert(od_reader<read_value, ctx>);
static_assert(od_reader<read_freq, ctx>);
static_assert(od_reader<read_public_field, ctx>);
static_assert(od_reader<read_checked, ctx>);
static_assert(od_reader<read_u8, ctx>);
static_assert(od_reader<zero<bool>, ctx>);
static_assert(od_reader<raw_read, ctx> == false);
static_assert(od_reader<[](ctx& c) { return c.commands; }, ctx>);
static_assert(od_reader<+[] { return 1.f; }, ctx>);
static_assert(!od_reader<read_double, ctx>);
static_assert(!od_reader<read_i64, ctx>);
static_assert(!od_reader<read_opaque, ctx>);
static_assert(!od_reader<read_other_error, ctx>);
static_assert(!od_reader<read_with_arg, ctx>);
static_assert(!od_reader<read_by_value, ctx>);
static_assert(!od_reader<[](auto&) { return 1.f; }, ctx>);
static_assert(!od_reader<read_value, opaque>);

static_assert(od_writer<write_value, ctx>);
static_assert(od_writer<write_freq, ctx>);
static_assert(od_writer<write_checked, ctx>);
static_assert(od_writer<[](float) {}, ctx>);
static_assert(!od_writer<write_by_reference, ctx>);
static_assert(!od_writer<write_returning, ctx>);
static_assert(!od_writer<write_nothing, ctx>);
static_assert(!od_writer<read_value, ctx>);

static_assert(od_command<command, ctx>);
static_assert(od_command<refuse, ctx>);
static_assert(od_command<+[] {}, ctx>);
static_assert(!od_command<command_returning, ctx>);
static_assert(!od_command<write_value, ctx>);

static_assert(od_string_reader<text, ctx>);
static_assert(od_string_reader<word, ctx>);
static_assert(!od_string_reader<read_value, ctx>);

static_assert(od_array_reader<samples, ctx>);
static_assert(od_array_reader<read_rates, ctx>);
static_assert(od_array_reader<read_longest, ctx>);
static_assert(!od_array_reader<read_too_long, ctx>);
static_assert(!od_array_reader<read_doubles, ctx>);
static_assert(!od_array_reader<read_unguarded, ctx>);
static_assert(!od_array_reader<read_value, ctx>);
static_assert(!od_array_reader<text, ctx>);

static_assert(od_rw_pair<read_value, write_value, ctx>);
static_assert(od_rw_pair<read_freq, write_value, ctx>);
static_assert(od_rw_pair<read_value, write_freq, ctx>);
static_assert(!od_rw_pair<read_value, write_checked, ctx>);
static_assert(!od_rw_pair<read_u8, write_value, ctx>);

// -- Bindings --

template<auto Binding>
consteval od_binding<ctx> bind()
{
  return Binding;
}

template<typename T>
consteval od_data_type read_type()
{
  return bind<od_ro<zero<T>>>().type;
}

static_assert(read_type<bool>() == od_data_type::boolean);
static_assert(read_type<std::int8_t>() == od_data_type::int8);
static_assert(read_type<std::int16_t>() == od_data_type::int16);
static_assert(read_type<std::int32_t>() == od_data_type::int32);
static_assert(read_type<std::uint8_t>() == od_data_type::uint8);
static_assert(read_type<std::uint16_t>() == od_data_type::uint16);
static_assert(read_type<std::uint32_t>() == od_data_type::uint32);
static_assert(read_type<float>() == od_data_type::float32);
static_assert(read_type<emb::units::hz_f32>() == od_data_type::float32);

static_assert(bind<od_ro<read_checked>>().type == od_data_type::float32);

static_assert(bind<od_ro<read_value>>().access == od_access::ro);
static_assert(bind<od_ro<read_value>>().read
              == &detail::od_read_thunk<read_value, ctx>);
static_assert(bind<od_ro<read_value>>().write == nullptr);
static_assert(bind<od_const<read_u8>>().access == od_access::const_);
static_assert(bind<od_const<read_u8>>().type == od_data_type::uint8);

static_assert(bind<od_wo<write_checked>>().access == od_access::wo);
static_assert(bind<od_wo<write_checked>>().type == od_data_type::int16);
static_assert(bind<od_wo<write_checked>>().read == nullptr);
static_assert(bind<od_wo<write_checked>>().write
              == &detail::od_write_thunk<write_checked, ctx>);
static_assert(bind<od_wo<write_freq>>().type == od_data_type::float32);

static_assert(bind<od_rw<read_freq, write_value>>().access == od_access::rw);
static_assert(bind<od_rw<read_freq, write_value>>().type
              == od_data_type::float32);
static_assert(bind<od_rw<read_freq, write_value>>().read
              == &detail::od_read_thunk<read_freq, ctx>);
static_assert(bind<od_rw<read_freq, write_value>>().write
              == &detail::od_write_thunk<write_value, ctx>);

static_assert(bind<od_exec<command>>().access == od_access::wo);
static_assert(bind<od_exec<command>>().type == od_data_type::uint32);
static_assert(bind<od_string<text>>().access == od_access::const_);
static_assert(bind<od_string<text>>().type == od_data_type::uint8);
static_assert(bind<od_string<text>>().count
              == &detail::od_string_count_thunk<text, ctx>);
static_assert(bind<od_ro_array<samples>>().access == od_access::ro);
static_assert(bind<od_ro_array<samples>>().type == od_data_type::float32);
static_assert(bind<od_ro_array<samples>>().read
              == &detail::od_array_element_thunk<samples, ctx>);
static_assert(bind<od_ro_array<samples>>().write == nullptr);
static_assert(bind<od_ro_array<samples>>().count
              == &detail::od_array_count_thunk<ctx, 3>);
static_assert(bind<od_ro_array<read_rates>>().type == od_data_type::float32);

// The symbols of the hertz and of the volt, in each language, and no unit.
inline constexpr emb::i18n::text hertz = {"Hz", "Гц"};
inline constexpr emb::i18n::text volt = {"V", "В"};
inline constexpr emb::i18n::text no_unit = {};

// The unit of an object is the symbol of the type of its value, in each
// language, if the type names one: of what the reader returns or the writer
// takes, of the elements of an array.
static_assert(bind<od_ro<read_freq>>().unit == hertz);
static_assert(bind<od_ro<read_value>>().unit == no_unit);
static_assert(bind<od_const<zero<emb::units::edeg_f32>>>().unit
              == emb::i18n::text{"\u00B0el", "\u00B0эл"});
static_assert(bind<od_wo<write_freq>>().unit == hertz);
static_assert(bind<od_ro_array<read_rates>>().unit == hertz);
static_assert(bind<od_ro_array<samples>>().unit == no_unit);
static_assert(bind<od_exec<command>>().unit == no_unit);
static_assert(bind<od_string<text>>().unit == no_unit);

// The reader and the writer of an rw object agree on the unit if one of them
// names none; the unit is the one named.
static_assert(bind<od_rw<read_freq, write_value>>().unit == hertz);
static_assert(bind<od_rw<read_value, write_freq>>().unit == hertz);
static_assert(bind<od_rw<read_freq, write_freq>>().unit == hertz);
static_assert(bind<od_rw<read_value, write_value>>().unit == no_unit);
// A reader of rpm and a writer of hertz travel as the same scalar, but
// disagree on the unit: the binding is refused with a message of its own.
static_assert(od_rw_pair<zero<emb::units::rpm_f32>, write_freq, ctx>);
static_assert(
    !detail::od_units_agree<zero<emb::units::rpm_f32>, write_freq, ctx>);
static_assert(detail::od_units_agree<read_freq, write_value, ctx>);
static_assert(detail::od_rw_unit_disagreement
              != detail::od_rw_type_disagreement);

// A command and an array of characters are marked as such, a command with
// its signature, if it takes one.
static_assert(bind<od_exec<command>>().command);
static_assert(!bind<od_exec<command>>().signature);
static_assert(bind<od_exec<command, od_save_signature>>().signature
              == od_save_signature);
static_assert(bind<od_string<text>>().text);
static_assert(!bind<od_string<text>>().command);
static_assert(!bind<od_ro<read_value>>().command);
static_assert(!bind<od_ro<read_value>>().text);

static_assert(!bind<od_ro<read_value>>().diagnosed);
static_assert(bind<od_ro<read_value>>().catalog == nullptr);

// -- Calling through bindings --

consteval bool test_reads()
{
  ctx c{.value = 1.5f};
  if (bind<od_ro<read_value>>().read(c, 0) != od_read_result{1.5f}) {
    return false;
  }
  // A wrapped value travels as its scalar.
  if (bind<od_ro<read_freq>>().read(c, 0) != od_read_result{1.5f}) {
    return false;
  }
  if (bind<od_ro<read_public_field>>().read(c, 0) != od_read_result{1.5f}) {
    return false;
  }
  if (bind<od_ro<read_checked>>().read(c, 0) != od_read_result{1.5f}) {
    return false;
  }
  c.fail = true;
  auto const failed = bind<od_ro<read_checked>>().read(c, 0);
  if (failed || failed.error() != sdo_abort_code::object_not_found) {
    return false;
  }
  if (bind<od_ro<read_u8>>().read(c, 0) != od_read_result{std::uint8_t{200}}) {
    return false;
  }
  return true;
}

static_assert(test_reads());

consteval bool test_writes()
{
  ctx c;
  if (!bind<od_wo<write_value>>().write(c, 0, od_value{2.5f})) return false;
  if (c.value != 2.5f) return false;

  // The writer gets the wrapped value.
  if (!bind<od_wo<write_freq>>().write(c, 0, od_value{3.5f})) return false;
  if (c.value != 3.5f) return false;

  auto const refused =
      bind<od_wo<write_checked>>().write(c, 0, od_value{std::int16_t{-1}});
  if (refused || refused.error() != sdo_abort_code::value_too_low) {
    return false;
  }
  if (c.value != 3.5f) return false;

  auto const mismatch =
      bind<od_wo<write_checked>>().write(c, 0, od_value{1.0f});
  if (mismatch || mismatch.error() != sdo_abort_code::data_type_mismatch) {
    return false;
  }
  return true;
}

static_assert(test_writes());

consteval bool test_commands()
{
  ctx c;
  if (!bind<od_exec<command>>().write(c, 0, od_value{std::uint32_t{0}})) {
    return false;
  }
  if (c.commands != 1) return false;

  auto const refused =
      bind<od_exec<refuse>>().write(c, 0, od_value{std::uint32_t{0}});
  return !refused && (refused.error() == sdo_abort_code::state_error);
}

static_assert(test_commands());

// A command that takes a signature runs on it alone; any other value is
// refused, and the command does not run.
static_assert(bind<od_exec<command, od_save_signature>>().access
              == od_access::wo);
static_assert(bind<od_exec<command, od_save_signature>>().type
              == od_data_type::uint32);

consteval bool test_signed_commands()
{
  ctx c;
  auto const save = bind<od_exec<command, od_save_signature>>();
  auto const zero = save.write(c, 0, od_value{std::uint32_t{0}});
  if (zero || zero.error() != sdo_abort_code::data_store_error) return false;
  auto const load = save.write(c, 0, od_value{od_load_signature});
  if (load || load.error() != sdo_abort_code::data_store_error) return false;
  if (c.commands != 0) return false;
  if (!save.write(c, 0, od_value{od_save_signature})) return false;
  return c.commands == 1;
}

static_assert(test_signed_commands());

// The signatures are the ASCII of their words, first character lowest.
static_assert(od_save_signature
              == (std::uint32_t{'s'}
                  | std::uint32_t{'a'} << 8
                  | std::uint32_t{'v'} << 16
                  | std::uint32_t{'e'} << 24));
static_assert(od_load_signature
              == (std::uint32_t{'l'}
                  | std::uint32_t{'o'} << 8
                  | std::uint32_t{'a'} << 16
                  | std::uint32_t{'d'} << 24));

// A string is an array of its characters: the count is its length, and each
// position one character; a position past the length is no element.
consteval bool test_string()
{
  ctx c;
  auto const string = bind<od_string<text>>();
  if (string.count(c, 0) != od_read_result{std::uint8_t{7}}) return false;
  if (string.read(c, 0) != od_read_result{std::uint8_t{'a'}}) return false;
  if (string.read(c, 6) != od_read_result{std::uint8_t{'g'}}) return false;
  if (string.read(c, 7)
      != std::unexpected(sdo_abort_code::subindex_not_found)) {
    return false;
  }

  auto const pointer = bind<od_string<word>>();
  if (pointer.count(c, 0) != od_read_result{std::uint8_t{4}}) return false;
  return pointer.read(c, 3) == od_read_result{std::uint8_t{'d'}};
}

static_assert(test_string());

// An array that a reader returns is served the same way: the count is its
// length, and each position one element, which travels as its scalar.
consteval bool test_ro_array()
{
  ctx c;
  auto const array = bind<od_ro_array<samples>>();
  if (array.count(c, 0) != od_read_result{std::uint8_t{3}}) return false;
  if (array.read(c, 0) != od_read_result{1.5f}) return false;
  if (array.read(c, 2) != od_read_result{3.5f}) return false;
  if (array.read(c, 3) != std::unexpected(sdo_abort_code::subindex_not_found)) {
    return false;
  }

  auto const wrapped = bind<od_ro_array<read_rates>>();
  if (wrapped.count(c, 0) != od_read_result{std::uint8_t{2}}) return false;
  return wrapped.read(c, 1) == od_read_result{60.f};
}

static_assert(test_ro_array());

// -- Enumerations --

// A gear of a drive, its values with a gap, labelled for a host.
enum class gear : std::uint8_t { park, drive, reverse = 4 };

consteval std::span<emb::enumerator_info<gear> const> enum_info(gear)
{
  static constexpr emb::enumerator_info<gear> info[] = {
      {gear::park, "park", {"Park", "Стоянка"}},
      {gear::drive, "drive", {"Drive", "Вперёд"}},
      {gear::reverse, "reverse", {"Reverse", "Назад"}},
  };
  return info;
}

// A tilt, below zero and without a gap.
enum class tilt : std::int8_t { left = -1, level, right };

consteval std::span<emb::enumerator_info<tilt> const> enum_info(tilt)
{
  static constexpr emb::enumerator_info<tilt> info[] = {
      {tilt::left, "left", {"Left", "Влево"}},
      {tilt::level, "level", {"Level", "Ровно"}},
      {tilt::right, "right", {"Right", "Вправо"}},
  };
  return info;
}

// An enumeration that does not label its values, and one too wide for the bus.
enum class unlabelled : std::uint8_t { a };
enum class wide : std::uint64_t { a };

constexpr gear read_gear(ctx const& c)
{
  return static_cast<gear>(static_cast<std::uint8_t>(c.value));
}

constexpr void write_gear(ctx& c, gear g)
{
  c.value = static_cast<float>(std::to_underlying(g));
}

constexpr std::expected<tilt, sdo_abort_code> read_tilt(ctx const& c)
{
  if (c.fail) return std::unexpected(sdo_abort_code::no_data_available);
  return tilt::left;
}

constexpr void write_tilt(ctx& c, tilt t)
{
  c.value = static_cast<float>(std::to_underlying(t));
}

constexpr std::array<gear, 2> gears()
{
  return {gear::drive, gear::reverse};
}

constexpr void write_u32(ctx& c, std::uint32_t v)
{
  c.value = static_cast<float>(v);
}

constexpr unlabelled read_unlabelled()
{
  return unlabelled::a;
}

constexpr wide read_wide()
{
  return wide::a;
}

// An enumeration travels as its carrier, its underlying value widened to 32
// bits of its signedness.
static_assert(od_enumeration<gear>);
static_assert(od_enumeration<tilt>);
static_assert(od_enumeration<unlabelled>);
static_assert(!od_enumeration<wide>);
static_assert(!od_enumeration<std::uint32_t>);
static_assert(std::same_as<od_carrier_t<gear>, std::uint32_t>);
static_assert(std::same_as<od_carrier_t<tilt>, std::int32_t>);
static_assert(std::same_as<od_carrier_t<std::int16_t>, std::int16_t>);
static_assert(std::same_as<od_carrier_t<emb::units::hz_f32>, float>);

consteval bool test_enumeration_values()
{
  if (to_od_value(gear::reverse) != od_value{std::uint32_t{4}}) return false;
  if (to_od_value(tilt::left) != od_value{std::int32_t{-1}}) return false;
  od_value const four{std::uint32_t{4}};
  od_value const minus_one{std::int32_t{-1}};
  od_value const past_a_byte{std::uint32_t{260}};
  od_value const byte{std::uint8_t{4}};
  if (from_od_value<gear>(four) != gear::reverse) return false;
  if (from_od_value<tilt>(minus_one) != tilt::left) return false;
  // 260 is not taken for the 4 its low byte holds, nor a uint8 for a gear.
  return !from_od_value<gear>(past_a_byte) && !from_od_value<gear>(byte);
}

static_assert(test_enumeration_values());

// The shapes of the handlers take an enumeration of at most 32 bits, labelled
// or not: the builders ask for the info of its enumerators.
static_assert(od_reader<read_gear, ctx>);
static_assert(od_reader<read_tilt, ctx>);
static_assert(od_reader<read_unlabelled, ctx>);
static_assert(!od_reader<read_wide, ctx>);
static_assert(od_writer<write_gear, ctx>);
static_assert(od_array_reader<gears, ctx>);
static_assert(od_rw_pair<read_gear, write_gear, ctx>);
static_assert(detail::od_values_agree<read_gear, write_gear, ctx>);
// A gear and a uint32 travel as one scalar but disagree on the values: od_rw
// refuses the pair with a message of its own.
static_assert(od_rw_pair<read_gear, write_u32, ctx>);
static_assert(!detail::od_values_agree<read_gear, write_u32, ctx>);
static_assert(!detail::od_values_agree<read_tilt, write_gear, ctx>);
static_assert(detail::od_values_agree<read_value, write_freq, ctx>);
static_assert(detail::od_rw_values_disagreement
              != detail::od_rw_type_disagreement);

// The binding of an enumeration takes the type of its carrier and the table
// of its enumerators; a number has none.
static_assert(bind<od_ro<read_gear>>().type == od_data_type::uint32);
static_assert(bind<od_ro<read_tilt>>().type == od_data_type::int32);
static_assert(bind<od_wo<write_gear>>().type == od_data_type::uint32);
static_assert(bind<od_rw<read_gear, write_gear>>().type
              == od_data_type::uint32);
static_assert(bind<od_ro_array<gears>>().type == od_data_type::uint32);
static_assert(bind<od_ro<read_gear>>().value_table.size() == 3);
static_assert(bind<od_ro<read_gear>>().value_table[2].value == 4);
static_assert(bind<od_ro<read_gear>>().value_table[2].name == "reverse");
static_assert(bind<od_ro<read_gear>>().value_table[2].label
              == emb::i18n::text{"Reverse", "Назад"});
static_assert(bind<od_ro<read_tilt>>().value_table[0].value == -1);
static_assert(bind<od_wo<write_gear>>().value_table.size() == 3);
static_assert(bind<od_rw<read_gear, write_gear>>().value_table.size() == 3);
static_assert(bind<od_ro_array<gears>>().value_table.size() == 3);
static_assert(bind<od_ro<read_value>>().value_table.empty());
static_assert(bind<od_ro<read_gear>>().unit == no_unit);
static_assert(!bind<od_ro<read_gear>>().diagnosed);

// A reader gives the carrier; a writer takes only the values the enumeration
// labels, checked in the carrier, and another type is a mismatch.
consteval bool test_enumerations()
{
  ctx c;
  auto const write = bind<od_wo<write_gear>>().write;
  auto const exceeded = std::unexpected(sdo_abort_code::value_range_exceeded);
  if (!write(c, 0, od_value{std::uint32_t{4}}) || c.value != 4.f) return false;
  if (bind<od_ro<read_gear>>().read(c, 0) != od_read_result{std::uint32_t{4}}) {
    return false;
  }
  // 260 would be a reverse once cut to its byte, and 2 lies in the gap.
  if (write(c, 0, od_value{std::uint32_t{260}}) != exceeded) return false;
  if (write(c, 0, od_value{std::uint32_t{2}}) != exceeded) return false;
  if (write(c, 0, od_value{std::uint8_t{1}})
      != std::unexpected(sdo_abort_code::data_type_mismatch)) {
    return false;
  }
  if (c.value != 4.f) return false;

  // Without a gap, the bounds tell the values named.
  auto const write_tilted = bind<od_wo<write_tilt>>().write;
  if (!write_tilted(c, 0, od_value{std::int32_t{-1}}) || c.value != -1.f) {
    return false;
  }
  if (write_tilted(c, 0, od_value{std::int32_t{2}}) != exceeded) return false;
  if (write_tilted(c, 0, od_value{std::int32_t{-2}}) != exceeded) return false;

  // A reader of std::expected passes its error through.
  if (bind<od_ro<read_tilt>>().read(c, 0) != od_read_result{std::int32_t{-1}}) {
    return false;
  }
  c.fail = true;
  if (bind<od_ro<read_tilt>>().read(c, 0)
      != std::unexpected(sdo_abort_code::no_data_available)) {
    return false;
  }

  auto const second = bind<od_ro_array<gears>>().read(c, 1);
  return second == od_read_result{std::uint32_t{4}};
}

static_assert(test_enumerations());

// -- Readers of a value that may be absent --

constexpr std::optional<emb::units::hz_f32> read_maybe_freq(ctx const& c)
{
  if (c.fail) return std::nullopt;
  return emb::units::hz_f32{c.value};
}

constexpr std::optional<gear> read_maybe_gear(ctx const& c)
{
  if (c.fail) return std::nullopt;
  return gear::reverse;
}

constexpr std::optional<std::array<float, 2>> maybe_samples()
{
  return std::nullopt;
}

constexpr std::array<std::optional<float>, 2> maybe_elements()
{
  return {};
}

// A reader of an object may return std::optional: the object's type, unit and
// table are those of the value it holds. The elements of an array may not
// be optional, nor the array.
static_assert(od_reader<read_maybe_freq, ctx>);
static_assert(od_reader<read_maybe_gear, ctx>);
static_assert(!od_array_reader<maybe_samples, ctx>);
static_assert(!od_array_reader<maybe_elements, ctx>);
static_assert(bind<od_ro<read_maybe_freq>>().type == od_data_type::float32);
static_assert(bind<od_ro<read_maybe_freq>>().unit == hertz);
static_assert(bind<od_const<read_maybe_freq>>().access == od_access::const_);
static_assert(bind<od_ro<read_maybe_gear>>().type == od_data_type::uint32);
static_assert(bind<od_ro<read_maybe_gear>>().value_table.size() == 3);
static_assert(od_rw_pair<read_maybe_freq, write_value, ctx>);
static_assert(bind<od_rw<read_maybe_freq, write_freq>>().unit == hertz);

// An empty value answers no_data_available, 0800 0024h; a value is read as a
// reader of the value itself reads it.
consteval bool test_absent_values()
{
  ctx c{.value = 1.5f};
  auto const no_data = std::unexpected(sdo_abort_code::no_data_available);
  if (bind<od_ro<read_maybe_freq>>().read(c, 0) != od_read_result{1.5f}) {
    return false;
  }
  if (bind<od_ro<read_maybe_gear>>().read(c, 0)
      != od_read_result{std::uint32_t{4}}) {
    return false;
  }
  if (bind<od_rw<read_maybe_freq, write_freq>>().read(c, 0)
      != od_read_result{1.5f}) {
    return false;
  }
  c.fail = true;
  if (bind<od_ro<read_maybe_freq>>().read(c, 0) != no_data) return false;
  if (bind<od_ro<read_maybe_gear>>().read(c, 0) != no_data) return false;
  if (bind<od_rw<read_maybe_freq, write_freq>>().read(c, 0) != no_data) {
    return false;
  }
  // The writer of the pair writes as any writer does.
  return bind<od_rw<read_maybe_freq, write_freq>>().write(c, 0, od_value{2.5f})
      && c.value == 2.5f;
}

static_assert(test_absent_values());

// -- Dictionary --

// The areas and the groups of the dictionaries below.
inline constexpr od_area areas[] = {
    {0x2000, 0x20FF, od_class::about},
    {0x2100, 0x2FFF, od_class::control},
    {0x3000, 0x4FFF, od_class::parameters},
    {0x5000, 0x5FFF, od_class::watch},
};

inline constexpr od_group groups[] = {
    {"a", {"Group A", "Группа А"}},
    {"b", {"Group B", "Группа Б"}},
};

// The info of an object of the tables below, labelled after its path.
consteval od_object_info labelled(std::string_view path)
{
  return {path, {path, path}};
}

consteval bool test_dictionary()
{
  static constexpr od_binding<ctx> by_hand{.access = od_access::rw,
                                           .type = od_data_type::float32,
                                           .read = &raw_read,
                                           .write = &raw_write,
                                           .arg = 7};
  // clang-format off
  static constexpr od_row<ctx> rows[] = {
      {{0x5000, 0x02}, labelled("b.value"),                          od_ro<read_value>},
      {{0x2100, 0x01}, labelled("a.command"),                        od_exec<command>},
      {{0x5000, 0x01}, labelled("b.checked"),                        od_ro<read_checked>},
      {{0x3000, 0x01}, {"a.freq", {"Frequency", "Частота"}},       od_rw<read_freq, write_value>},
      {{0x3000, 0x02}, labelled("a.raw"),                            by_hand},
  };
  // clang-format on

  auto const dictionary = make_dictionary<rows, areas, groups>();
  od_view<ctx> const view{dictionary};

  // Each of the three indices gets its subindex 00h.
  auto const entries = view.entries();
  if (entries.size() != 8) return false;
  for (auto i = 1uz; i < entries.size(); ++i) {
    if (!(entries[i - 1].key() < entries[i].key())) return false;
  }
  if (entries.front().key() != od_key{0x2100, 0x00}) return false;
  if (entries.back().key() != od_key{0x5000, 0x02}) return false;

  if (!view.records().empty() || !view.arrays().empty()) return false;

  // An index that rows serve lacks only the subindex; any other, the object.
  auto const no_subindex = std::unexpected(sdo_abort_code::subindex_not_found);
  auto const no_object = std::unexpected(sdo_abort_code::object_not_found);
  if (view.find({0x5000, 0x03}) != no_subindex) return false;
  if (view.find({0x0FFF, 0x00}) != no_object) return false;
  if (view.find({0x4000, 0x01}) != no_object) return false;
  if (view.find({0xFFFF, 0xFF}) != no_object) return false;

  auto const raw = view.find({0x3000, 0x02});
  if (!raw || (raw->arg != 7)) return false;

  // Subindex 00h of an index whose objects start past it reads the highest
  // subindex of the index.
  ctx c;
  auto const highest = [&](std::uint16_t index) {
    auto const last = view.find({index, 0x00});
    if (!last || (last->access != od_access::const_) || (last->type != uint8)) {
      return od_read_result{};
    }
    return last->read(c, last->arg);
  };
  if (highest(0x5000) != od_read_result{std::uint8_t{2}}) return false;
  if (highest(0x3000) != od_read_result{std::uint8_t{2}}) return false;
  if (highest(0x2100) != od_read_result{std::uint8_t{1}}) return false;

  auto const freq = view.find({0x3000, 0x01});
  if (!freq || (freq->access != od_access::rw)) return false;
  if (!freq->write(c, freq->arg, od_value{4.5f})) return false;
  if (freq->read(c, freq->arg) != od_read_result{4.5f}) return false;
  return raw->read(c, raw->arg) == od_read_result{4.5f};
}

static_assert(test_dictionary());

// A record serves every subindex of its index through its layout, which
// records share: each member gets the record's argument.
consteval bool test_records()
{
  // clang-format off
  static constexpr od_row<ctx> rows[] = {
      {{0x4001, 0x00}, labelled("a.second"), record(2)},
      {{0x5000, 0x01}, labelled("b.value"),  od_ro<read_value>},
      {{0x4000, 0x00}, labelled("a.first"),  record(1)},
  };
  // clang-format on

  auto const dictionary = make_dictionary<rows, areas, groups>();
  od_view<ctx> const view{dictionary};

  if (view.entries().size() != 2) return false;
  auto const records = view.records();
  if ((records.size() != 2) || (records[0].index != 0x4000)) return false;
  if (records[1].arg != 2) return false;

  ctx c;
  auto const last = view.find({0x4000, 0x00});
  if (!last || (last->access != od_access::const_) || (last->type != uint8)) {
    return false;
  }
  if (last->read(c, last->arg) != od_read_result{std::uint8_t{2}}) return false;

  auto const value = view.find({0x4001, 0x01});
  if (!value || (value->key() != od_key{0x4001, 0x01})) return false;
  if (!value->write(c, value->arg, od_value{2.5f})) return false;
  if (value->read(c, value->arg) != od_read_result{2.5f}) return false;

  auto const read_member = [&](od_key key) {
    auto const member = view.find(key);
    return member ? member->read(c, member->arg) : od_read_result{};
  };
  if (read_member({0x4000, 0x02}) != od_read_result{std::uint16_t{1}}) {
    return false;
  }
  if (read_member({0x4001, 0x02}) != od_read_result{std::uint16_t{2}}) {
    return false;
  }

  if (view.find({0x4000, 0x03})
      != std::unexpected(sdo_abort_code::subindex_not_found)) {
    return false;
  }
  return view.find({0x4002, 0x00})
      == std::unexpected(sdo_abort_code::object_not_found);
}

static_assert(test_records());

// An array serves every subindex of its index: 00h reads the count, and any
// other the element at the subindex less one, which its handler may refuse.
consteval bool test_arrays()
{
  // clang-format off
  static constexpr od_row<ctx> rows[] = {
      {{0x2001, 0x00}, labelled("a.word"),    od_string<word>},
      {{0x5001, 0x00}, labelled("b.samples"), od_ro_array<samples>},
      {{0x5000, 0x01}, labelled("b.value"),   od_ro<read_value>},
      {{0x2000, 0x00}, labelled("a.text"),    od_string<text>},
  };
  // clang-format on

  auto const dictionary = make_dictionary<rows, areas, groups>();
  od_view<ctx> const view{dictionary};

  if (view.entries().size() != 2) return false;
  auto const arrays = view.arrays();
  if ((arrays.size() != 3) || (arrays[0].index != 0x2000)) return false;

  ctx c;
  auto const count = view.find({0x2000, 0x00});
  if (!count
      || (count->access != od_access::const_)
      || (count->type != uint8)) {
    return false;
  }
  if (count->read(c, count->arg) != od_read_result{std::uint8_t{7}}) {
    return false;
  }

  auto const first = view.find({0x2000, 0x01});
  if (!first || (first->key() != od_key{0x2000, 0x01}) || (first->arg != 0)) {
    return false;
  }
  if (first->read(c, first->arg) != od_read_result{std::uint8_t{'a'}}) {
    return false;
  }

  auto const past = view.find({0x2000, 0x08});
  if (!past
      || past->read(c, past->arg)
             != std::unexpected(sdo_abort_code::subindex_not_found)) {
    return false;
  }

  auto const last = view.find({0x2001, 0x04});
  if (!last || last->read(c, last->arg) != od_read_result{std::uint8_t{'d'}}) {
    return false;
  }

  auto const sample = view.find({0x5001, 0x03});
  if (!sample
      || (sample->access != od_access::ro)
      || (sample->type != float32)
      || (sample->read(c, sample->arg) != od_read_result{3.5f})) {
    return false;
  }
  auto const beyond = view.find({0x5001, 0x04});
  if (!beyond
      || beyond->read(c, beyond->arg)
             != std::unexpected(sdo_abort_code::subindex_not_found)) {
    return false;
  }
  return view.find({0x2002, 0x00})
      == std::unexpected(sdo_abort_code::object_not_found);
}

static_assert(test_arrays());

// -- Description --

// The 32-bit FNV-1a hash of `text`, as the fingerprint hashes the body of a
// description.
consteval std::uint32_t fnv1a(std::string_view text)
{
  std::uint32_t h = 0x811C9DC5u;
  for (char c : text) {
    h ^= static_cast<std::uint8_t>(c);
    h *= 0x01000193u;
  }
  return h;
}

// The groups of the tables below, in an order the description does not keep.
inline constexpr od_group described_groups[] = {
    {"b", {"Group B", "Группа Б"}},
    {"a", {"Group A", "Группа А"}},
};

inline constexpr od_group regrouped[] = {described_groups[1],
                                         described_groups[0]};

// clang-format off
inline constexpr od_row<ctx> described[] = {
    {{0x5000, 0x01}, {"b.value", {"Value", "Значение"}, {"V", "В"}}, od_ro<read_value>},
    {{0x2100, 0x01}, {"a.save", {"Save", "Сохранить"}},              od_exec<command, od_save_signature>},
    {{0x2000, 0x00}, {"a.text", {"Text", "Текст"}},                  od_string<text>},
    {{0x4000, 0x00}, {"a.first", {"First", "Первый"}, {"A", "А"}},   record(1)},
    {{0x5001, 0x00}, {"b.rates", {"Rates", "Частоты"}},              od_ro_array<read_rates>},
};
// clang-format on

inline constexpr od_row<ctx> reordered[] = {described[3],
                                            described[0],
                                            described[4],
                                            described[2],
                                            described[1]};

// The body of the description of `described`: groups by their first rows,
// the members of the layout, then the rows by key, each line canonical.
inline constexpr std::string_view described_body =
    "{\"type\":\"group\",\"name\":\"a\",\"label_en\":\"Group A\","
    "\"label_ru\":\"Группа А\"}\n"
    "{\"type\":\"group\",\"name\":\"b\",\"label_en\":\"Group B\","
    "\"label_ru\":\"Группа Б\"}\n"
    "{\"type\":\"field\",\"layout\":0,\"sub\":\"0x00\","
    "\"name\":\"highest_subindex\",\"label_en\":\"Highest subindex\","
    "\"label_ru\":\"Наибольший подындекс\",\"datatype\":\"uint8\","
    "\"access\":\"const\"}\n"
    "{\"type\":\"field\",\"layout\":0,\"sub\":\"0x01\",\"name\":\"value\","
    "\"label_en\":\"Value\",\"label_ru\":\"Значение\",\"access\":\"rw\","
    "\"inherits\":true}\n"
    "{\"type\":\"field\",\"layout\":0,\"sub\":\"0x02\",\"name\":\"arg\","
    "\"label_en\":\"Argument\",\"label_ru\":\"Аргумент\","
    "\"datatype\":\"uint16\",\"access\":\"ro\"}\n"
    "{\"type\":\"array\",\"index\":\"0x2000\",\"group\":\"a\","
    "\"name\":\"text\",\"label_en\":\"Text\",\"label_ru\":\"Текст\","
    "\"class\":\"about\",\"datatype\":\"uint8\",\"access\":\"const\","
    "\"text\":true}\n"
    "{\"type\":\"object\",\"index\":\"0x2100\",\"sub\":\"0x01\","
    "\"group\":\"a\",\"name\":\"save\",\"label_en\":\"Save\","
    "\"label_ru\":\"Сохранить\",\"class\":\"control\",\"datatype\":\"uint32\","
    "\"access\":\"wo\",\"command\":true,\"signature\":\"0x65766173\"}\n"
    "{\"type\":\"record\",\"index\":\"0x4000\",\"group\":\"a\","
    "\"name\":\"first\",\"label_en\":\"First\",\"label_ru\":\"Первый\","
    "\"class\":\"parameters\",\"datatype\":\"float32\",\"unit_en\":\"A\","
    "\"unit_ru\":\"А\",\"layout\":0}\n"
    "{\"type\":\"object\",\"index\":\"0x5000\",\"sub\":\"0x01\","
    "\"group\":\"b\",\"name\":\"value\",\"label_en\":\"Value\","
    "\"label_ru\":\"Значение\",\"class\":\"watch\",\"datatype\":\"float32\","
    "\"access\":\"ro\",\"unit_en\":\"V\",\"unit_ru\":\"В\"}\n"
    "{\"type\":\"array\",\"index\":\"0x5001\",\"group\":\"b\","
    "\"name\":\"rates\",\"label_en\":\"Rates\",\"label_ru\":\"Частоты\","
    "\"class\":\"watch\",\"datatype\":\"float32\",\"access\":\"ro\","
    "\"unit_en\":\"Hz\",\"unit_ru\":\"Гц\"}\n";

inline constexpr std::string_view described_header_start =
    "{\"type\":\"header\",\"format\":\"emb-canopen-od\",\"version\":1,"
    "\"device\":\"Test device\",\"fingerprint\":\"0x";

constexpr auto description =
    make_description<described, areas, described_groups, "Test device">();

consteval std::string_view as_view(auto const& chars)
{
  return {chars.data(), chars.size()};
}

// The description is its header, which states the fingerprint, then the
// body, whose hash the fingerprint is, the dictionary's.
static_assert(as_view(description).starts_with(described_header_start));
static_assert(as_view(description).ends_with(described_body));
static_assert(description.size()
              == described_header_start.size() + 11 + described_body.size());
static_assert(od_description_fingerprint(description) == fnv1a(described_body));
static_assert(
    make_dictionary<described, areas, described_groups>().fingerprint()
    == fnv1a(described_body));
consteval bool test_view_fingerprint()
{
  auto const dictionary = make_dictionary<described, areas, described_groups>();
  return od_view<ctx>{dictionary}.fingerprint() == fnv1a(described_body);
}

static_assert(test_view_fingerprint());

// Neither the order of the rows nor that of the groups changes the text.
static_assert(
    as_view(make_description<reordered, areas, regrouped, "Test device">())
    == as_view(description));

// The fingerprint follows the paths, the classes and the labels: a row
// renamed, relabelled or in an area of another class changes it.
inline constexpr od_area reclassed[] = {
    {0x2000, 0x20FF, od_class::about},
    {0x2100, 0x2FFF, od_class::control},
    {0x3000, 0x4FFF, od_class::watch},
    {0x5000, 0x5FFF, od_class::watch},
};

inline constexpr od_row<ctx> relabelled[] = {
    described[0],
    described[1],
    described[2],
    {{0x4000, 0x00}, {"a.first", {"First", "Первая"}, {"A", "А"}}, record(1)},
    described[4]};

static_assert(
    make_dictionary<described, reclassed, described_groups>().fingerprint()
    != fnv1a(described_body));
static_assert(
    make_dictionary<relabelled, areas, described_groups>().fingerprint()
    != fnv1a(described_body));

// -- Description: layouts --

constexpr od_read_result read_other_arg(ctx&, std::uint16_t arg)
{
  return od_value{static_cast<std::uint16_t>(arg + 1)};
}

// `record_layout` with another reader of the argument, which a host does not
// see: the two layouts describe as one.
inline constexpr od_field<ctx> other_layout[] = {
    record_layout[0],
    record_layout[1],
    {.access = od_access::ro, .type = uint16, .read = &read_other_arg},
};

// A record of a uint8 whose value is a const member, as 00h and 07h of the
// record of a parameter are, and with a command that takes a signature.
inline constexpr od_field<ctx> byte_layout[] = {
    {.access = od_access::const_, .type = uint8, .read = &read_last_subindex},
    {.access = od_access::const_, .type = uint8, .read = &read_last_subindex},
    {.access = od_access::wo, .type = uint32, .write = &raw_write},
};

inline constexpr od_field_info byte_field_info[] = {
    {.name = "highest_subindex",
     .label = {"Highest subindex", "Наибольший подындекс"}},
    {.name = "value", .label = {"Value", "Значение"}, .inherits = true},
    {.name = "save",
     .label = {"Save", "Сохранить"},
     .command = true,
     .signature = od_save_signature},
};

// clang-format off
inline constexpr od_row<ctx> layouts[] = {
    {{0x4002, 0x00}, labelled("a.byte"),   {.access = od_access::ro, .type = uint8, .layout = byte_layout, .field_info = byte_field_info}},
    {{0x4001, 0x00}, labelled("a.other"),  {.access = od_access::ro, .type = float32, .arg = 2, .layout = other_layout, .field_info = record_field_info}},
    {{0x4000, 0x00}, labelled("a.first"),  record(1)},
    {{0x5000, 0x01}, labelled("b.value"),  od_ro<read_value>},
};
// clang-format on

constexpr auto described_layouts =
    make_description<layouts, areas, groups, "Test device">();

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

// Two layouts, one of them twice: 3 and 3 members.
static_assert(occurrences(as_view(described_layouts), "\"type\":\"field\"")
              == 6);
static_assert(occurrences(as_view(described_layouts), "\"layout\":0}") == 2);
static_assert(occurrences(as_view(described_layouts), "\"layout\":1}") == 1);
// The const member that inherits the value is told from 00h by the flag, and
// the command shows its signature.
static_assert(as_view(described_layouts)
                  .contains("{\"type\":\"field\",\"layout\":1,\"sub\":\"0x00\","
                            "\"name\":\"highest_subindex\",\"label_en\":"
                            "\"Highest subindex\",\"label_ru\":\"Наибольший "
                            "подындекс\",\"datatype\":\"uint8\",\"access\":"
                            "\"const\"}\n"));
static_assert(as_view(described_layouts)
                  .contains("{\"type\":\"field\",\"layout\":1,\"sub\":\"0x01\","
                            "\"name\":\"value\",\"label_en\":\"Value\","
                            "\"label_ru\":\"Значение\",\"access\":\"const\","
                            "\"inherits\":true}\n"));
static_assert(
    as_view(described_layouts)
        .contains("{\"type\":\"field\",\"layout\":1,\"sub\":\"0x02\","
                  "\"name\":\"save\",\"label_en\":\"Save\","
                  "\"label_ru\":\"Сохранить\",\"datatype\":\"uint32\","
                  "\"access\":\"wo\",\"command\":true,"
                  "\"signature\":\"0x65766173\"}\n"));
static_assert(as_view(described_layouts)
                  .contains("{\"type\":\"record\",\"index\":\"0x4002\","
                            "\"group\":\"a\",\"name\":\"byte\","
                            "\"label_en\":\"a.byte\",\"label_ru\":\"a.byte\","
                            "\"class\":\"parameters\",\"datatype\":\"uint8\","
                            "\"layout\":1}\n"));

// -- Description: values --

constexpr od_read_result read_byte(ctx&, std::uint16_t)
{
  return od_value{std::uint8_t{1}};
}

constexpr od_read_result read_level(ctx&, std::uint16_t)
{
  return od_value{std::int16_t{-1}};
}

// The values of a policy, of a mode and of a level, named by hand; a level
// may be below zero or past a byte.
inline constexpr od_value_info policies[] = {
    {0, "on_command", {"On command", "По команде"}},
    {1, "on_restart", {"On restart", "После перезапуска"}},
};

inline constexpr od_value_info modes[] = {
    {0, "off", {"Off", "Выкл."}},
    {1, "on", {"On", "Вкл."}},
};

inline constexpr od_value_info levels[] = {
    {-1, "low", {"Low", "Низкий"}},
    {300, "high", {"High", "Высокий"}},
};

// The values of `modes` in an array of their own: the same table for a host.
inline constexpr od_value_info same_modes[] = {modes[0], modes[1]};

// A record of a named uint8 whose member 02h, a policy, names values of its
// own.
inline constexpr od_field<ctx> mode_layout[] = {
    {.access = od_access::const_, .type = uint8, .read = &read_last_subindex},
    {.access = od_access::ro, .type = uint8, .read = &read_byte},
    {.access = od_access::const_, .type = uint8, .read = &read_byte},
};

inline constexpr od_field_info mode_field_info[] = {
    {.name = "highest_subindex",
     .label = {"Highest subindex", "Наибольший подындекс"}},
    {.name = "value", .label = {"Value", "Значение"}, .inherits = true},
    {.name = "policy",
     .label = {"Policy", "Политика"},
     .value_table = policies},
};

consteval od_binding<ctx>
mode_record(std::span<od_field_info const> info = mode_field_info)
{
  return {.access = od_access::ro,
          .type = uint8,
          .layout = mode_layout,
          .field_info = info,
          .value_table = same_modes};
}

inline constexpr od_binding<ctx> level_object{.access = od_access::ro,
                                              .type = int16,
                                              .read = &read_level,
                                              .value_table = levels};

inline constexpr od_binding<ctx> mode_array{.access = od_access::ro,
                                            .type = uint8,
                                            .read = &read_byte,
                                            .count = &read_last_subindex,
                                            .value_table = modes};

// The rows of named values, not in the order of their keys.
// clang-format off
inline constexpr od_row<ctx> valued[] = {
    {{0x5002, 0x01}, labelled("b.level"), level_object},
    {{0x5003, 0x00}, labelled("b.modes"), mode_array},
    {{0x4003, 0x00}, labelled("a.mode"),  mode_record()},
};
// clang-format on

inline constexpr od_row<ctx> revalued[] = {valued[2], valued[0], valued[1]};

// The body of the description of `valued`: the values of the tables after
// the groups, by table and by value, the tables numbered as the members of
// the layouts and then the rows by key first name them, each once, though
// `modes` and `same_modes` are two arrays; then the members and the rows,
// each that has a table referring to it.
inline constexpr std::string_view valued_body =
    "{\"type\":\"group\",\"name\":\"a\",\"label_en\":\"Group A\","
    "\"label_ru\":\"Группа А\"}\n"
    "{\"type\":\"group\",\"name\":\"b\",\"label_en\":\"Group B\","
    "\"label_ru\":\"Группа Б\"}\n"
    "{\"type\":\"value\",\"value_table\":0,\"value\":0,"
    "\"name\":\"on_command\",\"label_en\":\"On command\","
    "\"label_ru\":\"По команде\"}\n"
    "{\"type\":\"value\",\"value_table\":0,\"value\":1,"
    "\"name\":\"on_restart\",\"label_en\":\"On restart\","
    "\"label_ru\":\"После перезапуска\"}\n"
    "{\"type\":\"value\",\"value_table\":1,\"value\":0,\"name\":\"off\","
    "\"label_en\":\"Off\",\"label_ru\":\"Выкл.\"}\n"
    "{\"type\":\"value\",\"value_table\":1,\"value\":1,\"name\":\"on\","
    "\"label_en\":\"On\",\"label_ru\":\"Вкл.\"}\n"
    "{\"type\":\"value\",\"value_table\":2,\"value\":-1,\"name\":\"low\","
    "\"label_en\":\"Low\",\"label_ru\":\"Низкий\"}\n"
    "{\"type\":\"value\",\"value_table\":2,\"value\":300,"
    "\"name\":\"high\",\"label_en\":\"High\",\"label_ru\":\"Высокий\"}\n"
    "{\"type\":\"field\",\"layout\":0,\"sub\":\"0x00\","
    "\"name\":\"highest_subindex\",\"label_en\":\"Highest subindex\","
    "\"label_ru\":\"Наибольший подындекс\",\"datatype\":\"uint8\","
    "\"access\":\"const\"}\n"
    "{\"type\":\"field\",\"layout\":0,\"sub\":\"0x01\",\"name\":\"value\","
    "\"label_en\":\"Value\",\"label_ru\":\"Значение\",\"access\":\"ro\","
    "\"inherits\":true}\n"
    "{\"type\":\"field\",\"layout\":0,\"sub\":\"0x02\",\"name\":\"policy\","
    "\"label_en\":\"Policy\",\"label_ru\":\"Политика\",\"datatype\":\"uint8\","
    "\"access\":\"const\",\"value_table\":0}\n"
    "{\"type\":\"record\",\"index\":\"0x4003\",\"group\":\"a\","
    "\"name\":\"mode\",\"label_en\":\"a.mode\",\"label_ru\":\"a.mode\","
    "\"class\":\"parameters\",\"datatype\":\"uint8\",\"value_table\":1,"
    "\"layout\":0}\n"
    "{\"type\":\"object\",\"index\":\"0x5002\",\"sub\":\"0x01\","
    "\"group\":\"b\",\"name\":\"level\",\"label_en\":\"b.level\","
    "\"label_ru\":\"b.level\",\"class\":\"watch\",\"datatype\":\"int16\","
    "\"access\":\"ro\",\"value_table\":2}\n"
    "{\"type\":\"array\",\"index\":\"0x5003\",\"group\":\"b\","
    "\"name\":\"modes\",\"label_en\":\"b.modes\",\"label_ru\":\"b.modes\","
    "\"class\":\"watch\",\"datatype\":\"uint8\",\"access\":\"ro\","
    "\"value_table\":1}\n";

constexpr auto valued_description =
    make_description<valued, areas, groups, "Test device">();

static_assert(as_view(valued_description).starts_with(described_header_start));
static_assert(as_view(valued_description).ends_with(valued_body));
static_assert(valued_description.size()
              == described_header_start.size() + 11 + valued_body.size());
static_assert(od_description_fingerprint(valued_description)
              == fnv1a(valued_body));
static_assert(make_dictionary<valued, areas, groups>().fingerprint()
              == fnv1a(valued_body));
static_assert(
    as_view(make_description<revalued, areas, groups, "Test device">())
    == as_view(valued_description));

// Layouts that differ in the table of a member are two; those whose info is
// two arrays of the same content are one.
inline constexpr od_field_info same_mode_field_info[] = {mode_field_info[0],
                                                         mode_field_info[1],
                                                         mode_field_info[2]};

inline constexpr od_field_info other_mode_field_info[] = {
    mode_field_info[0],
    mode_field_info[1],
    {.name = "policy", .label = {"Policy", "Политика"}, .value_table = modes},
};

// clang-format off
inline constexpr od_row<ctx> policy_layouts[] = {
    {{0x4003, 0x00}, labelled("a.mode"),  mode_record()},
    {{0x4004, 0x00}, labelled("a.same"),  mode_record(same_mode_field_info)},
    {{0x4005, 0x00}, labelled("a.other"), mode_record(other_mode_field_info)},
    {{0x5002, 0x01}, labelled("b.level"), level_object},
};
// clang-format on

constexpr auto described_policies =
    make_description<policy_layouts, areas, groups, "Test device">();

static_assert(occurrences(as_view(described_policies), "\"type\":\"field\"")
              == 6);
static_assert(occurrences(as_view(described_policies), "\"layout\":0}") == 2);
static_assert(occurrences(as_view(described_policies), "\"layout\":1}") == 1);
// The tables: `policies`, then `modes` of the other layout, which is also the
// table of the three records, then `levels`.
static_assert(occurrences(as_view(described_policies), "\"type\":\"value\"")
              == 6);
static_assert(as_view(described_policies)
                  .contains("\"name\":\"policy\",\"label_en\":\"Policy\","
                            "\"label_ru\":\"Политика\",\"datatype\":\"uint8\","
                            "\"access\":\"const\",\"value_table\":1}\n"));
static_assert(occurrences(as_view(described_policies), "\"value_table\":1,")
              == 5);

// An object of an enumeration names its values with the info of its
// enumerators.
// clang-format off
inline constexpr od_row<ctx> geared[] = {
    {{0x5002, 0x01}, labelled("b.gear"), od_ro<read_gear>},
    {{0x4003, 0x00}, labelled("a.mode"), mode_record()},
};
// clang-format on

constexpr auto described_gears =
    make_description<geared, areas, groups, "Test device">();

static_assert(as_view(described_gears)
                  .contains("{\"type\":\"value\",\"value_table\":2,\"value\":4,"
                            "\"name\":\"reverse\",\"label_en\":\"Reverse\","
                            "\"label_ru\":\"Назад\"}\n"));
static_assert(as_view(described_gears)
                  .ends_with("\"group\":\"b\",\"name\":\"gear\","
                             "\"label_en\":\"b.gear\",\"label_ru\":\"b.gear\","
                             "\"class\":\"watch\",\"datatype\":\"uint32\","
                             "\"access\":\"ro\",\"value_table\":2}\n"));

// -- Checks --

// The one group of the tables below.
inline constexpr od_group group_a[] = {{"a", {"Group A", "Группа А"}}};

// Builds the binding of an object from its parts, for the checks below.
consteval od_binding<ctx>
raw(od_access access, od_data_type type, bool reader, bool writer)
{
  return {.access = access,
          .type = type,
          .read = reader ? &raw_read : nullptr,
          .write = writer ? &raw_write : nullptr};
}

consteval std::string check_valid()
{
  // clang-format off
  static constexpr od_row<ctx> rows[] = {
      {{0x2100, 0x01}, labelled("a.command"), od_exec<command>},
      {{0x5000, 0x01}, labelled("a.value"),   od_ro<read_value>},
  };
  // clang-format on
  return detail::od_check(rows, areas, group_a);
}

static_assert(check_valid().empty());

consteval std::string check_reserved_index()
{
  // clang-format off
  static constexpr od_row<ctx> rows[] = {
      {{0x0FFF, 0x01}, labelled("a.c"), od_ro<read_value>},
  };
  // clang-format on
  return detail::od_check(rows, areas, group_a);
}

static_assert(check_reserved_index()
              == "od: 0FFFh:01 a.c: indices below 1000h are reserved");

consteval std::string check_one(od_binding<ctx> binding)
{
  od_row<ctx> const rows[] = {{{0x2000, 0x01}, labelled("a.c"), binding}};
  return detail::od_check(rows, areas, group_a);
}

static_assert(check_one(raw(od_access::ro, od_data_type::float32, false, false))
              == "od: 2000h:01 a.c: readable but has no reader");
static_assert(check_one(raw(od_access::wo, od_data_type::float32, true, true))
              == "od: 2000h:01 a.c: write-only but has a reader");
static_assert(check_one(raw(od_access::rw, od_data_type::float32, true, false))
              == "od: 2000h:01 a.c: writable but has no writer");
static_assert(check_one(raw(od_access::ro, od_data_type::float32, true, true))
              == "od: 2000h:01 a.c: read-only but has a writer");

// An index must lie in an area.
consteval std::string check_outside()
{
  // clang-format off
  static constexpr od_row<ctx> rows[] = {
      {{0x6000, 0x01}, labelled("a.c"), od_ro<read_value>},
  };
  // clang-format on
  return detail::od_check(rows, areas, group_a);
}

static_assert(check_outside() == "od: 6000h:01 a.c: the index lies in no area");

// -- Checks: info of objects --

// Checks the info `info` of an object at 2000h:01.
consteval std::string check_info(od_object_info info)
{
  od_row<ctx> const rows[] = {{{0x2000, 0x01}, info, od_ro<read_value>}};
  return detail::od_check(rows, areas, group_a);
}

inline constexpr std::string_view not_a_path =
    ": the path must be a group and a name, each a letter followed by "
    "letters, digits or underscores";

static_assert(check_info(labelled("a.c")).empty());
static_assert(check_info(labelled("a.Vdc_2")).empty());
static_assert(check_info(labelled("a"))
              == "od: 2000h:01 a" + std::string{not_a_path});
static_assert(check_info(labelled("a.c.d"))
              == "od: 2000h:01 a.c.d" + std::string{not_a_path});
static_assert(check_info(labelled("a.1c"))
              == "od: 2000h:01 a.1c" + std::string{not_a_path});
static_assert(check_info(labelled("a._c"))
              == "od: 2000h:01 a._c" + std::string{not_a_path});
static_assert(check_info(labelled("a.c-d"))
              == "od: 2000h:01 a.c-d" + std::string{not_a_path});
static_assert(check_info(labelled(".c"))
              == "od: 2000h:01 .c" + std::string{not_a_path});
static_assert(check_info(labelled("a/c"))
              == "od: 2000h:01 a/c" + std::string{not_a_path});
static_assert(check_info({}) == "od: 2000h:01" + std::string{not_a_path});
static_assert(check_info(labelled("x.c"))
              == "od: 2000h:01 x.c: group 'x' is not declared");

// Checks the label `en`, `ru` of an object at 2000h:01.
consteval std::string check_label(std::string_view en, std::string_view ru)
{
  return check_info({"a.c", {en, ru}});
}

inline constexpr std::string_view not_verbatim =
    "od: 2000h:01 a.c: the label must be UTF-8 without quotation marks, "
    "backslashes or control characters";

static_assert(check_label("Motor temperature, °C", "Температура, °C").empty());
static_assert(check_label("Resistance, Ω", "Сопротивление, Ω").empty());
static_assert(check_label("Smile \U0001F600", "Улыбка \U0001F600").empty());
static_assert(check_label("C", "")
              == "od: 2000h:01 a.c: the label must not be empty in any "
                 "language");
static_assert(check_label("", "Ц")
              == "od: 2000h:01 a.c: the label must not be empty in any "
                 "language");
static_assert(check_label("The \"c\"", "Ц") == not_verbatim);
static_assert(check_label("C", "Ц\\") == not_verbatim);
static_assert(check_label("C\n", "Ц") == not_verbatim);
static_assert(check_label("C\x7F", "Ц") == not_verbatim);
// U+0085, a control character of Latin-1.
static_assert(check_label("C\xC2\x85", "Ц") == not_verbatim);
// UTF-8 cut short, a stray continuation byte, a byte that never starts a
// character, the overlong form of '/', a surrogate and a code point past
// U+10FFFF.
static_assert(check_label("C\xC3", "Ц") == not_verbatim);
static_assert(check_label("C\x80", "Ц") == not_verbatim);
static_assert(check_label("C\xFF", "Ц") == not_verbatim);
static_assert(check_label("C\xC0\xAF", "Ц") == not_verbatim);
static_assert(check_label("C\xED\xA0\x80", "Ц") == not_verbatim);
static_assert(check_label("C\xF4\x90\x80\x80", "Ц") == not_verbatim);

// A binding that gives the info of its object, as that of a settings
// parameter does.
consteval od_binding<ctx> named()
{
  return {.access = od_access::ro,
          .type = od_data_type::float32,
          .read = &raw_read,
          .info = {"a.named", {"Named", "Названный"}}};
}

static_assert(check_one(named())
              == "od: 2000h:01 a.named: the row describes the object, but "
                 "its binding does too");

consteval std::string check_named(od_object_info info)
{
  od_row<ctx> const rows[] = {{{0x2000, 0x01}, info, named()}};
  return detail::od_check(rows, areas, group_a);
}

static_assert(check_named({}).empty());
static_assert(check_named({"", {}, volt})
              == "od: 2000h:01 a.named: the row describes the object, but "
                 "its binding does too");

// -- Checks: units --

// Checks the object at 2000h:00, of the path "a.c" and the unit `unit`,
// bound by `binding`.
consteval std::string check_unit(emb::i18n::text unit, od_binding<ctx> binding)
{
  od_row<ctx> const rows[] = {
      {{0x2000, 0x00}, {"a.c", {"C", "Ц"}, unit}, binding}};
  return detail::od_check(rows, areas, group_a);
}

// A unit for a value whose type names none, or in place of the symbol of the
// type, which the info must not repeat.
static_assert(check_unit(volt, od_ro<read_value>).empty());
static_assert(check_unit({"rpm", "об/мин"}, od_ro<read_freq>).empty());
static_assert(check_unit(no_unit, od_ro<read_freq>).empty());
static_assert(check_unit(hertz, od_ro<read_freq>)
              == "od: 2000h:00 a.c: the unit 'Hz' repeats the symbol of the "
                 "value's type");
static_assert(check_unit(hertz, od_ro_array<read_rates>)
              == "od: 2000h:00 a.c: the unit 'Hz' repeats the symbol of the "
                 "value's type");
// Another symbol in one language is another unit, not a repetition.
static_assert(check_unit({"Hz", "Герц"}, od_ro<read_freq>).empty());
// No unit for a command, an array of characters or a boolean.
static_assert(check_unit(volt, od_exec<command>)
              == "od: 2000h:00 a.c: a command has no unit");
static_assert(check_unit(volt, od_string<text>)
              == "od: 2000h:00 a.c: an array of characters has no unit");
static_assert(check_unit(volt, od_ro<zero<bool>>)
              == "od: 2000h:00 a.c: a boolean has no unit");
// A symbol in each language, each verbatim: an empty one or a quotation mark
// in any language breaks the unit.
static_assert(check_unit({"V", ""}, od_ro<read_value>)
              == "od: 2000h:00 a.c: the unit must not be empty in any "
                 "language");
static_assert(check_unit({"", "В"}, od_ro<read_value>)
              == "od: 2000h:00 a.c: the unit must not be empty in any "
                 "language");
static_assert(check_unit({"\"V\"", "В"}, od_ro<read_value>)
              == "od: 2000h:00 a.c: the unit must be UTF-8 without quotation "
                 "marks, backslashes or control characters");
static_assert(check_unit({"V", "\"В\""}, od_ro<read_value>)
              == "od: 2000h:00 a.c: the unit must be UTF-8 without quotation "
                 "marks, backslashes or control characters");
static_assert(check_unit({"\u03A9", "Ом"}, od_ro<read_value>).empty());

// -- Checks: areas and groups --

// Checks the object at 2000h:01, of the path "a.c", against `in`.
template<std::size_t A>
consteval std::string check_in(od_area const (&in)[A])
{
  od_row<ctx> const rows[] = {
      {{0x2000, 0x01}, labelled("a.c"), od_ro<read_value>}};
  return detail::od_check(rows, in, group_a);
}

consteval std::string check_backward()
{
  od_area const in[] = {{0x2000, 0x20FF, od_class::about},
                        {0x3000, 0x2FFF, od_class::watch}};
  return check_in(in);
}

consteval std::string check_overlap()
{
  od_area const in[] = {{0x2080, 0x21FF, od_class::control},
                        {0x2000, 0x20FF, od_class::about}};
  return check_in(in);
}

consteval std::string check_adjacent()
{
  od_area const in[] = {{0x2100, 0x21FF, od_class::control},
                        {0x2000, 0x20FF, od_class::about}};
  return check_in(in);
}

static_assert(check_backward()
              == "od: area 3000h-2FFFh: the first index is past the last");
static_assert(check_overlap()
              == "od: areas 2000h-20FFh and 2080h-21FFh overlap");
static_assert(check_adjacent().empty());

// Checks the object at 2000h:01, of the path "a.c", with the group "a" and
// `group` declared.
consteval std::string check_group(od_group group)
{
  od_row<ctx> const rows[] = {
      {{0x2000, 0x01}, labelled("a.c"), od_ro<read_value>}};
  od_group const with[] = {group_a[0], group};
  return detail::od_check(rows, areas, with);
}

inline constexpr std::string_view not_a_name =
    "': the name must be a letter followed by letters, digits or "
    "underscores";

static_assert(check_group({"1b", {"B", "Б"}})
              == "od: group '1b" + std::string{not_a_name});
static_assert(check_group({"b.c", {"B", "Б"}})
              == "od: group 'b.c" + std::string{not_a_name});
static_assert(check_group({"", {"B", "Б"}})
              == "od: group '" + std::string{not_a_name});
static_assert(check_group({"watch", {"Watch", "Наблюдение"}})
              == "od: group 'watch': named as a class");
static_assert(check_group({"parameters", {"Parameters", "Параметры"}})
              == "od: group 'parameters': named as a class");
static_assert(check_group({"status", {"Status", "Статус"}})
              == "od: group 'status': named as a class");
static_assert(check_group({"a", {"A again", "Снова А"}})
              == "od: group 'a': declared twice");
static_assert(check_group({"b", {"B", ""}})
              == "od: group 'b': the label must not be empty in any "
                 "language");
static_assert(check_group({"b", {"B\t", "Б"}})
              == "od: group 'b': the label must be UTF-8 without quotation "
                 "marks, backslashes or control characters");
static_assert(check_group({"b", {"B", "Б"}}) == "od: group 'b': has no row");

// A record is checked field by field, at the row that names it.
consteval std::string check_record(od_binding<ctx> binding)
{
  od_row<ctx> const rows[] = {{{0x2000, 0x00}, labelled("a.c"), binding}};
  return detail::od_check(rows, areas, group_a);
}

inline constexpr od_field<ctx> unread_layout[] = {
    {.access = od_access::const_, .type = uint8, .read = &read_last_subindex},
    {.access = od_access::ro, .type = float32},
};

static_assert(check_record(record(1)).empty());
static_assert(check_one(record(1))
              == "od: 2000h:01 a.c: a record's row must have subindex 00");
static_assert(check_record({.access = od_access::ro,
                            .type = float32,
                            .read = &raw_read,
                            .layout = record_layout})
              == "od: 2000h:00 a.c: a record has no handlers of its own");
static_assert(check_record({.access = od_access::ro,
                            .type = float32,
                            .layout = unread_layout})
              == "od: 2000h:00 a.c: member 01h readable but has no reader");

// The members of a record are described: info for each, named, labelled,
// a member that inherits the value of the record's type, and a signature for
// a command only.
consteval std::string check_member_info(std::span<od_field_info const> info,
                                        emb::i18n::text unit = {})
{
  od_row<ctx> const rows[] = {{{0x2000, 0x00},
                               {"a.c", {"C", "Ц"}, unit},
                               {.access = od_access::ro,
                                .type = float32,
                                .layout = record_layout,
                                .field_info = info}}};
  return detail::od_check(rows, areas, group_a);
}

// The info of `record_layout`, the member at `at` replaced with `member`.
consteval std::array<od_field_info, 3> replaced(std::size_t at,
                                                od_field_info member)
{
  std::array<od_field_info, 3> out{record_field_info[0],
                                   record_field_info[1],
                                   record_field_info[2]};
  out[at] = member;
  return out;
}

static_assert(check_member_info(record_field_info).empty());
static_assert(check_member_info(std::span{record_field_info}.first(2))
              == "od: 2000h:00 a.c: the record has 3 members but describes 2");
static_assert(check_member_info(replaced(1,
                                         {.name = "2nd", .label = {"V", "З"}}))
              == "od: 2000h:00 a.c: member 01h: the name must be a letter "
                 "followed by letters, digits or underscores");
static_assert(check_member_info(replaced(2,
                                         {.name = "highest_subindex",
                                          .label = {"A", "А"}}))
              == "od: 2000h:00 a.c: members 00h and 02h have the same name");
static_assert(check_member_info(replaced(1,
                                         {.name = "value", .label = {"V", ""}}))
              == "od: 2000h:00 a.c: member 01h: the label must not be empty in "
                 "any language");
static_assert(
    check_member_info(replaced(1, {.name = "value", .label = {"V\"", "З"}}))
    == "od: 2000h:00 a.c: member 01h: the label must be UTF-8 without "
       "quotation marks, backslashes or control characters");
static_assert(check_member_info(replaced(2,
                                         {.name = "arg",
                                          .label = {"A", "А"},
                                          .signature = od_save_signature}))
              == "od: 2000h:00 a.c: member 02h has a signature but is no "
                 "command");
static_assert(check_member_info(replaced(
                  2,
                  {.name = "arg", .label = {"A", "А"}, .inherits = true}))
              == "od: 2000h:00 a.c: member 02h inherits the record's type, "
                 "float32, but is uint16");
// A record whose value no member holds has no unit, nor does its line state
// a type.
static_assert(
    check_member_info(replaced(1, {.name = "value", .label = {"V", "З"}}),
                      {"A", "А"})
    == "od: 2000h:00 a.c: a record without a member that inherits "
       "its value has no unit");

inline constexpr std::array<od_field_info, 3> plain_field_info =
    replaced(1, {.name = "value", .label = {"Value", "Значение"}});

inline constexpr od_row<ctx> plain[] = {
    {{0x4000, 0x00},
     labelled("a.plain"),
     {.access = od_access::ro,
      .type = float32,
      .layout = record_layout,
      .field_info = plain_field_info}},
};

static_assert(
    as_view(make_description<plain, areas, group_a, "Test device">())
        .ends_with("{\"type\":\"record\",\"index\":\"0x4000\","
                   "\"group\":\"a\",\"name\":\"plain\","
                   "\"label_en\":\"a.plain\",\"label_ru\":\"a.plain\","
                   "\"class\":\"parameters\",\"layout\":0}\n"));

// An array is checked at the row that names it, its elements as one object.
static_assert(check_record(od_string<text>).empty());
static_assert(check_record(od_ro_array<samples>).empty());
static_assert(check_one(od_string<text>)
              == "od: 2000h:01 a.c: an array's row must have subindex 00");
static_assert(
    check_record({.access = od_access::ro, .type = uint8, .count = &raw_read})
    == "od: 2000h:00 a.c: element readable but has no reader");
static_assert(check_record({.access = od_access::ro,
                            .type = float32,
                            .count = &raw_read,
                            .layout = record_layout})
              == "od: 2000h:00 a.c: a binding is a record or an array, not "
                 "both");

inline constexpr std::string_view items[] = {"x", "y"};
inline constexpr bool all_exposed[] = {true, true};
inline constexpr bool y_hidden[] = {true, false};
inline constexpr od_catalog catalog{"item", items, all_exposed};
inline constexpr od_catalog hiding_catalog{"item", items, y_hidden};

consteval od_binding<ctx> item(std::uint16_t arg, od_catalog const& from)
{
  return {.access = od_access::ro,
          .type = od_data_type::float32,
          .read = &raw_read,
          .arg = arg,
          .catalog = &from};
}

static_assert(check_one(item(2, catalog))
              == "od: 2000h:01 a.c: arg is outside its catalog");

consteval std::string check_two(od_row<ctx> first, od_row<ctx> second)
{
  od_row<ctx> const rows[] = {first, second};
  return detail::od_check(rows, areas, group_a);
}

static_assert(check_two({{0x2000, 0x01}, labelled("a.c"), od_ro<read_value>},
                        {{0x2000, 0x01}, labelled("a.d"), od_ro<read_u8>})
              == "od: 2000h:01 a.c and 2000h:01 a.d have the same key");
static_assert(check_two({{0x2000, 0x01}, labelled("a.c"), od_ro<read_value>},
                        {{0x2000, 0x02}, labelled("a.c"), od_ro<read_u8>})
              == "od: 2000h:01 a.c and 2000h:02 a.c have the same path");
static_assert(
    check_two({{0x2000, 0x00}, labelled("a.c"), record(1)},
              {{0x2000, 0x05}, labelled("a.d"), od_ro<read_u8>})
    == "od: 2000h:00 a.c and 2000h:05 a.d share the index of a record");
static_assert(check_two({{0x2000, 0x00}, labelled("a.c"), record(1)},
                        {{0x2001, 0x00}, labelled("a.d"), record(1)})
                  .empty());
static_assert(
    check_two({{0x2000, 0x00}, labelled("a.c"), od_string<text>},
              {{0x2000, 0x05}, labelled("a.d"), od_ro<read_u8>})
    == "od: 2000h:00 a.c and 2000h:05 a.d share the index of an array");
static_assert(
    check_two({{0x2000, 0x05}, labelled("a.d"), od_ro<read_u8>},
              {{0x2000, 0x00}, labelled("a.c"), od_ro<read_value>})
    == "od: 2000h:05 a.d and 2000h:00 a.c share an index whose subindex 00 "
       "the dictionary serves");
// Checks two objects that differ in key and path, 2000h:01 a.c and
// 2000h:02 a.d.
consteval std::string check_pair(od_binding<ctx> first, od_binding<ctx> second)
{
  return check_two({{0x2000, 0x01}, labelled("a.c"), first},
                   {{0x2000, 0x02}, labelled("a.d"), second});
}

consteval od_binding<ctx> reading(std::uint16_t arg)
{
  return {.access = od_access::ro,
          .type = od_data_type::float32,
          .read = &raw_read,
          .arg = arg};
}

static_assert(check_pair(od_ro<read_value>, od_ro<read_value>)
              == "od: 2000h:01 a.c and 2000h:02 a.d are read by the same "
                 "handler");
static_assert(check_pair(od_wo<write_value>, od_wo<write_value>)
              == "od: 2000h:01 a.c and 2000h:02 a.d are written by the same "
                 "handler");
// The same handler with different arguments serves different objects.
static_assert(check_pair(reading(0), reading(1)).empty());

static_assert(check_pair(item(0, catalog), item(0, catalog))
              == "od: item 'x' has two rows: 2000h:01 a.c and 2000h:02 a.d");
static_assert(check_one(item(0, catalog)) == "od: item 'y' has no row");
static_assert(check_pair(item(0, catalog), item(1, catalog)).empty());
static_assert(check_pair(item(0, hiding_catalog), item(1, hiding_catalog))
              == "od: item 'y' is not exposed but has a row: 2000h:02 a.d");
static_assert(check_one(item(0, hiding_catalog)).empty());

// A builder that has reported its error silences the checks.
consteval od_binding<ctx> diagnosed()
{
  return {.access = od_access::ro,
          .type = od_data_type::uint32,
          .diagnosed = true};
}

static_assert(check_two({{0x0FFF, 0x01}, labelled("a.c"), od_ro<read_value>},
                        {{0x2000, 0x02}, labelled("a.d"), diagnosed()})
                  .empty());

// -- Checks: values --

// Checks the object at 2000h:00, of the path "a.c" and the unit `unit`, bound
// by `binding` and its values named by `values`.
consteval std::string check_table(od_binding<ctx> binding,
                                  std::span<od_value_info const> values,
                                  emb::i18n::text unit = {})
{
  binding.value_table = values;
  od_row<ctx> const rows[] = {
      {{0x2000, 0x00}, {"a.c", {"C", "Ц"}, unit}, binding}};
  return detail::od_check(rows, areas, group_a);
}

consteval od_binding<ctx> raw_reader(od_data_type type)
{
  return raw(od_access::ro, type, true, false);
}

inline constexpr od_value_info bytes_past[] = {
    {0, "zero", {"Zero", "Ноль"}},
    {256, "past", {"Past a byte", "За байтом"}},
};

inline constexpr od_value_info signed_bytes[] = {
    {-128, "lowest", {"Lowest", "Наименьшее"}},
    {127, "highest", {"Highest", "Наибольшее"}},
};

// An object, an array or a record of an integer may name its values, each a
// value of the type.
static_assert(check_table(raw_reader(uint8), modes).empty());
static_assert(check_table(raw_reader(int8), signed_bytes).empty());
static_assert(check_table(raw_reader(int16), levels).empty());
static_assert(check_table(mode_array, modes).empty());
static_assert(check_table(mode_record(), modes).empty());
static_assert(check_table(raw_reader(uint8), bytes_past)
              == "od: 2000h:00 a.c: value 256 does not fit uint8");
static_assert(check_table(raw_reader(uint32), levels)
              == "od: 2000h:00 a.c: value -1 does not fit uint32");
static_assert(check_table(mode_record(), bytes_past)
              == "od: 2000h:00 a.c: value 256 does not fit uint8");
// No table for a float32, a boolean, a command or an array of characters, nor
// for an array of float32.
static_assert(check_table(raw_reader(float32), modes)
              == "od: 2000h:00 a.c: a float32 has no table of values");
static_assert(check_table(raw_reader(boolean), modes)
              == "od: 2000h:00 a.c: a boolean has no table of values");
static_assert(check_table(od_exec<command>, modes)
              == "od: 2000h:00 a.c: a command has no table of values");
static_assert(check_table(od_string<text>, modes)
              == "od: 2000h:00 a.c: an array of characters has no table of "
                 "values");
static_assert(check_table(od_ro_array<samples>, modes)
              == "od: 2000h:00 a.c: a float32 has no table of values");
// A named value needs no unit.
static_assert(check_table(raw_reader(uint8), modes, volt)
              == "od: 2000h:00 a.c: values that a table names have no unit");

// The info of `byte_layout` whose member 01h holds no value of the record.
inline constexpr od_field_info byte_plain_field_info[] = {
    byte_field_info[0],
    {.name = "value", .label = {"Value", "Значение"}},
    byte_field_info[2],
};

// A record names the values of its members that inherit its value, and so
// has a table only if one does.
static_assert(check_table({.access = od_access::ro,
                           .type = uint8,
                           .layout = byte_layout,
                           .field_info = byte_plain_field_info},
                          modes)
              == "od: 2000h:00 a.c: a record without a member that inherits "
                 "its value has no table of values");

// A member names its values, unless it inherits those of the record, in a
// type that takes a table, each a value of the type.
static_assert(check_member_info(replaced(2,
                                         {.name = "arg",
                                          .label = {"A", "А"},
                                          .value_table = modes}))
                  .empty());
static_assert(check_member_info(replaced(1,
                                         {.name = "value",
                                          .label = {"V", "З"},
                                          .inherits = true,
                                          .value_table = modes}))
              == "od: 2000h:00 a.c: member 01h inherits the record's values "
                 "and has no table of its own");
static_assert(
    check_member_info(
        replaced(1,
                 {.name = "value", .label = {"V", "З"}, .value_table = modes}))
    == "od: 2000h:00 a.c: member 01h: a float32 has no table of values");
static_assert(check_member_info(replaced(2,
                                         {.name = "arg",
                                          .label = {"A", "А"},
                                          .command = true,
                                          .value_table = modes}))
              == "od: 2000h:00 a.c: member 02h: a command has no table of "
                 "values");
static_assert(check_member_info(replaced(
                  2,
                  {.name = "arg", .label = {"A", "А"}, .value_table = levels}))
              == "od: 2000h:00 a.c: member 02h: value -1 does not fit uint16");

// Checks the values `first` and `second` of a uint8 object at 2000h:00.
consteval std::string check_two_values(od_value_info first,
                                       od_value_info second)
{
  od_value_info const values[] = {first, second};
  return check_table(raw_reader(uint8), values);
}

// A table names each value as a segment of a path, labels it, and holds the
// values in increasing order, no two with the same name or with the same
// label in one language: two labels "A (001)" are a copy not corrected. The
// label of a value may be the same in both languages, as an abbreviation is.

static_assert(check_two_values({0, "svpwm", {"SVPWM", "SVPWM"}},
                               {1, "dpwm0", {"DPWM0", "DPWM0"}})
                  .empty());
static_assert(check_two_values({0, "a", {"A", "А"}}, {1, "2nd", {"B", "Б"}})
              == "od: 2000h:00 a.c: value 1: the name must be a letter "
                 "followed by letters, digits or underscores");
static_assert(check_two_values({0, "a", {"A", "А"}}, {1, "b", {"B", ""}})
              == "od: 2000h:00 a.c: value 1: the label must not be empty in "
                 "any language");
static_assert(check_two_values({0, "a", {"A", "А"}}, {1, "b", {"B\"", "Б"}})
              == "od: 2000h:00 a.c: value 1: the label must be UTF-8 without "
                 "quotation marks, backslashes or control characters");
static_assert(check_two_values({1, "b", {"B", "Б"}}, {0, "a", {"A", "А"}})
              == "od: 2000h:00 a.c: value 0 follows value 1: the values must "
                 "increase");
static_assert(check_two_values({1, "a", {"A", "А"}}, {1, "b", {"B", "Б"}})
              == "od: 2000h:00 a.c: value 1 follows value 1: the values must "
                 "increase");
static_assert(check_two_values({0, "a", {"A", "А"}}, {1, "a", {"B", "Б"}})
              == "od: 2000h:00 a.c: values 0 and 1 have the same name");
static_assert(check_two_values({0, "a", {"A (001)", "А (001)"}},
                               {1, "ab", {"A (001)", "АБ (011)"}})
              == "od: 2000h:00 a.c: values 0 and 1 have the same label in "
                 "English");
static_assert(check_two_values({0, "a", {"A", "Один"}}, {1, "b", {"B", "Один"}})
              == "od: 2000h:00 a.c: values 0 and 1 have the same label in "
                 "Russian");

// A table is checked once, and a broken one is reported at the first row that
// names its values with it, as at the first record whose member does.
inline constexpr od_value_info misnamed[] = {{0, "2nd", {"A", "А"}}};

inline constexpr od_field_info misnamed_field_info[] = {
    mode_field_info[0],
    mode_field_info[1],
    {.name = "policy",
     .label = {"Policy", "Политика"},
     .value_table = misnamed},
};

consteval std::string check_shared_tables()
{
  // clang-format off
  static constexpr od_row<ctx> rows[] = {
      {{0x5002, 0x01}, labelled("a.first"),  {.access = od_access::ro, .type = int16, .read = &read_level, .value_table = misnamed}},
      {{0x5002, 0x02}, labelled("a.second"), {.access = od_access::ro, .type = uint8, .read = &read_byte, .value_table = misnamed}},
  };
  // clang-format on
  return detail::od_check(rows, areas, group_a);
}

consteval std::string check_shared_member_tables()
{
  // clang-format off
  static constexpr od_row<ctx> rows[] = {
      {{0x4001, 0x00}, labelled("a.first"),  mode_record(misnamed_field_info)},
      {{0x4002, 0x00}, labelled("a.second"), mode_record(misnamed_field_info)},
  };
  // clang-format on
  return detail::od_check(rows, areas, group_a);
}

static_assert(check_shared_tables()
              == "od: 5002h:01 a.first: value 0: the name must be a letter "
                 "followed by letters, digits or underscores");
static_assert(check_shared_member_tables()
              == "od: 4001h:00 a.first: member 02h: value 0: the name must be "
                 "a letter followed by letters, digits or underscores");

// -- Description: PDOs --

using namespace std::chrono_literals;

using decivolt =
    emb::scaled<std::int16_t, emb::units::volt_f32, std::ratio<1, 10>>;

// What a TPDO reads out: a gear, whose values the object b.gear names too, a
// flag, a tilt, whose values no object names, a voltage and a counter.
struct report {
  gear drive_gear;
  bool on;
  tilt body_tilt;
  decivolt voltage;
  std::uint8_t counter = 0;
};

// The rows of the report, not in the order of their bits.
// clang-format off
inline constexpr pdo_row<report> report_rows[] = {
{{16, 16}, "voltage", {"Voltage", "Напряжение"}, pdo_map<&report::voltage>},
{{ 0,  3}, "gear", {"Gear", "Передача"},         pdo_map<&report::drive_gear>},
{{56,  2}, {}, {},                               pdo_map_counter<&report::counter>},
{{ 3,  1}, "on", {"On", "Включено"},             pdo_map<&report::on>},
{{ 4,  2}, "tilt", {"Tilt", "Наклон"},           pdo_map<&report::body_tilt>},
};
// clang-format on

// What an RPDO sets: a flag and a plain integer.
struct setpoint {
  bool stop;
  std::int16_t level;
};

// clang-format off
inline constexpr pdo_row<setpoint> setpoint_rows[] = {
{{0,  1}, "stop", {"Stop", "Стоп"},      pdo_map<&setpoint::stop>},
{{8, 16}, "level", {"Level", "Уровень"}, pdo_map<&setpoint::level>},
};
// clang-format on

// TPDO2 of the predefined form, and RPDO5 of a custom COB-ID.
inline constexpr od_pdo report_tpdo{.name = "report",
                                    .label = {"Report", "Отчёт"},
                                    .direction = pdo_direction::tx,
                                    .number = 2,
                                    .cob_id = pdo_id::predefined(),
                                    .period = 10ms};

inline constexpr od_pdo setpoint_rpdo{.name = "setpoint",
                                      .label = {"Setpoint", "Уставка"},
                                      .direction = pdo_direction::rx,
                                      .number = 5,
                                      .cob_id = pdo_id::custom(0x405),
                                      .timeout = 350ms};

inline constexpr auto report_pdo = pdo_of<report_rows>(report_tpdo);
inline constexpr auto setpoint_pdo = pdo_of<setpoint_rows>(setpoint_rpdo);

// The type of the PDO knows its rows, and the PDO converts to what the
// description reads: the signals of its rows.
inline constexpr od_mapped_pdo mapped_report = report_pdo;

static_assert(&mapped_list<report_pdo> == &report_rows);
static_assert(&mapped_report.pdo == &report_pdo.pdo);
static_assert(mapped_report.signals.size() == 5);
static_assert(mapped_report.signals[1].bits.start == 0);
static_assert(mapped_report.signals[2].role == pdo_role::counter);
static_assert(mapped_report.signals[0].scale_den == 10);
static_assert(mapped_report.signals[0].unit == volt);
static_assert(mapped_report.mapping() == pdo_mapping::signals);

// The PDOs beside the dictionary `geared`, the RPDO first.
inline constexpr od_mapped_pdo processed_pdos[] = {setpoint_pdo, report_pdo};
inline constexpr od_process_data processed{.pdos = processed_pdos};

// The body of the description of `geared` with the PDOs: the values of the
// tilt, which a signal names first, as table 3, after those of the
// dictionary, the signal of the gear sharing table 2 with b.gear; and after
// the objects, the TPDO, then the RPDO, each followed by its signals by first
// bit. The TPDO states the base of its COB-ID, the RPDO its custom COB-ID; a
// counter has no path or label, a voltage states its step and unit.
inline constexpr std::string_view processed_body =
    "{\"type\":\"group\",\"name\":\"a\",\"label_en\":\"Group A\","
    "\"label_ru\":\"Группа А\"}\n"
    "{\"type\":\"group\",\"name\":\"b\",\"label_en\":\"Group B\","
    "\"label_ru\":\"Группа Б\"}\n"
    "{\"type\":\"value\",\"value_table\":0,\"value\":0,"
    "\"name\":\"on_command\",\"label_en\":\"On command\","
    "\"label_ru\":\"По команде\"}\n"
    "{\"type\":\"value\",\"value_table\":0,\"value\":1,"
    "\"name\":\"on_restart\",\"label_en\":\"On restart\","
    "\"label_ru\":\"После перезапуска\"}\n"
    "{\"type\":\"value\",\"value_table\":1,\"value\":0,\"name\":\"off\","
    "\"label_en\":\"Off\",\"label_ru\":\"Выкл.\"}\n"
    "{\"type\":\"value\",\"value_table\":1,\"value\":1,\"name\":\"on\","
    "\"label_en\":\"On\",\"label_ru\":\"Вкл.\"}\n"
    "{\"type\":\"value\",\"value_table\":2,\"value\":0,\"name\":\"park\","
    "\"label_en\":\"Park\",\"label_ru\":\"Стоянка\"}\n"
    "{\"type\":\"value\",\"value_table\":2,\"value\":1,"
    "\"name\":\"drive\",\"label_en\":\"Drive\",\"label_ru\":\"Вперёд\"}\n"
    "{\"type\":\"value\",\"value_table\":2,\"value\":4,"
    "\"name\":\"reverse\",\"label_en\":\"Reverse\",\"label_ru\":\"Назад\"}\n"
    "{\"type\":\"value\",\"value_table\":3,\"value\":-1,"
    "\"name\":\"left\",\"label_en\":\"Left\",\"label_ru\":\"Влево\"}\n"
    "{\"type\":\"value\",\"value_table\":3,\"value\":0,"
    "\"name\":\"level\",\"label_en\":\"Level\",\"label_ru\":\"Ровно\"}\n"
    "{\"type\":\"value\",\"value_table\":3,\"value\":1,"
    "\"name\":\"right\",\"label_en\":\"Right\",\"label_ru\":\"Вправо\"}\n"
    "{\"type\":\"field\",\"layout\":0,\"sub\":\"0x00\","
    "\"name\":\"highest_subindex\",\"label_en\":\"Highest subindex\","
    "\"label_ru\":\"Наибольший подындекс\",\"datatype\":\"uint8\","
    "\"access\":\"const\"}\n"
    "{\"type\":\"field\",\"layout\":0,\"sub\":\"0x01\",\"name\":\"value\","
    "\"label_en\":\"Value\",\"label_ru\":\"Значение\",\"access\":\"ro\","
    "\"inherits\":true}\n"
    "{\"type\":\"field\",\"layout\":0,\"sub\":\"0x02\",\"name\":\"policy\","
    "\"label_en\":\"Policy\",\"label_ru\":\"Политика\",\"datatype\":\"uint8\","
    "\"access\":\"const\",\"value_table\":0}\n"
    "{\"type\":\"record\",\"index\":\"0x4003\",\"group\":\"a\","
    "\"name\":\"mode\",\"label_en\":\"a.mode\",\"label_ru\":\"a.mode\","
    "\"class\":\"parameters\",\"datatype\":\"uint8\",\"value_table\":1,"
    "\"layout\":0}\n"
    "{\"type\":\"object\",\"index\":\"0x5002\",\"sub\":\"0x01\","
    "\"group\":\"b\",\"name\":\"gear\",\"label_en\":\"b.gear\","
    "\"label_ru\":\"b.gear\",\"class\":\"watch\",\"datatype\":\"uint32\","
    "\"access\":\"ro\",\"value_table\":2}\n"
    "{\"type\":\"pdo\",\"name\":\"report\",\"label_en\":\"Report\","
    "\"label_ru\":\"Отчёт\",\"direction\":\"tx\",\"number\":2,"
    "\"mapping\":\"signals\",\"cob_base\":\"0x280\",\"period_ms\":10}\n"
    "{\"type\":\"signal\",\"pdo\":\"report\",\"start\":0,\"width\":3,"
    "\"role\":\"value\",\"name\":\"gear\",\"label_en\":\"Gear\","
    "\"label_ru\":\"Передача\",\"encoding\":\"unsigned\","
    "\"value_table\":2}\n"
    "{\"type\":\"signal\",\"pdo\":\"report\",\"start\":3,\"width\":1,"
    "\"role\":\"value\",\"name\":\"on\",\"label_en\":\"On\","
    "\"label_ru\":\"Включено\",\"encoding\":\"unsigned\"}\n"
    "{\"type\":\"signal\",\"pdo\":\"report\",\"start\":4,\"width\":2,"
    "\"role\":\"value\",\"name\":\"tilt\",\"label_en\":\"Tilt\","
    "\"label_ru\":\"Наклон\",\"encoding\":\"signed\",\"value_table\":3}\n"
    "{\"type\":\"signal\",\"pdo\":\"report\",\"start\":16,\"width\":16,"
    "\"role\":\"value\",\"name\":\"voltage\",\"label_en\":\"Voltage\","
    "\"label_ru\":\"Напряжение\",\"encoding\":\"signed\",\"scale\":\"1/10\","
    "\"unit_en\":\"V\",\"unit_ru\":\"В\"}\n"
    "{\"type\":\"signal\",\"pdo\":\"report\",\"start\":56,\"width\":2,"
    "\"role\":\"counter\",\"encoding\":\"unsigned\"}\n"
    "{\"type\":\"pdo\",\"name\":\"setpoint\",\"label_en\":\"Setpoint\","
    "\"label_ru\":\"Уставка\",\"direction\":\"rx\",\"number\":5,"
    "\"mapping\":\"signals\",\"cob_id\":\"0x405\",\"timeout_ms\":350}\n"
    "{\"type\":\"signal\",\"pdo\":\"setpoint\",\"start\":0,\"width\":1,"
    "\"role\":\"value\",\"name\":\"stop\",\"label_en\":\"Stop\","
    "\"label_ru\":\"Стоп\",\"encoding\":\"unsigned\"}\n"
    "{\"type\":\"signal\",\"pdo\":\"setpoint\",\"start\":8,\"width\":16,"
    "\"role\":\"value\",\"name\":\"level\",\"label_en\":\"Level\","
    "\"label_ru\":\"Уровень\",\"encoding\":\"signed\"}\n";

constexpr auto processed_description =
    make_description<geared, areas, groups, "Test device", processed>();

// The fingerprint, in the header and in the dictionary, is the hash of the
// whole body, the PDOs included.
static_assert(
    as_view(processed_description).starts_with(described_header_start));
static_assert(as_view(processed_description).ends_with(processed_body));
static_assert(processed_description.size()
              == described_header_start.size() + 11 + processed_body.size());
static_assert(od_description_fingerprint(processed_description)
              == fnv1a(processed_body));
static_assert(make_dictionary<geared, areas, groups, processed>().fingerprint()
              == fnv1a(processed_body));

// Neither the order of the PDOs nor that of the rows of a PDO changes the
// text.
// clang-format off
inline constexpr pdo_row<report> sorted_report_rows[] = {
    report_rows[1], report_rows[3], report_rows[4], report_rows[0], report_rows[2]};
// clang-format on

inline constexpr auto sorted_report_pdo =
    pdo_of<sorted_report_rows>(report_tpdo);
inline constexpr od_mapped_pdo reordered_pdos[] = {sorted_report_pdo,
                                                   setpoint_pdo};
inline constexpr od_process_data reprocessed{.pdos = reordered_pdos};

static_assert(
    as_view(
        make_description<geared, areas, groups, "Test device", reprocessed>())
    == as_view(processed_description));

// Without PDOs the text and the fingerprint are those of the dictionary
// alone, as they were before process data.
inline constexpr od_process_data no_pdos{};

static_assert(
    as_view(make_description<geared, areas, groups, "Test device", no_pdos>())
    == as_view(described_gears));
static_assert(as_view(make_description<geared,
                                       areas,
                                       groups,
                                       "Test device",
                                       detail::od_no_process_data>())
              == as_view(described_gears));
static_assert(make_dictionary<geared, areas, groups, no_pdos>().fingerprint()
              == make_dictionary<geared, areas, groups>().fingerprint());

// `report_tpdo` with what `change` changes.
consteval od_pdo changed(auto change)
{
  od_pdo pdo = report_tpdo;
  change(pdo);
  return pdo;
}

// The fingerprint follows the PDOs: one of another period changes it.
inline constexpr auto slower_report_pdo =
    pdo_of<report_rows>(changed([](od_pdo& pdo) { pdo.period = 20ms; }));
inline constexpr od_mapped_pdo slower_pdos[] = {setpoint_pdo,
                                                slower_report_pdo};
inline constexpr od_process_data slower{.pdos = slower_pdos};

static_assert(make_dictionary<geared, areas, groups, slower>().fingerprint()
              != fnv1a(processed_body));

// The base of the predefined COB-ID of a PDO, which a host adds the node-ID
// to: 180h + 100h (n - 1) for TPDO n and 200h + 100h (n - 1) for RPDO n.
static_assert(detail::cob_base_of(pdo_direction::tx, 1) == 0x180);
static_assert(detail::cob_base_of(pdo_direction::rx, 1) == 0x200);
static_assert(detail::cob_base_of(pdo_direction::rx, 2) == 0x300);
static_assert(detail::cob_base_of(pdo_direction::tx, 4) == 0x480);
static_assert(detail::cob_base_of(pdo_direction::rx, 4) == 0x500);

// -- Checks: PDOs --

// Checks the PDO `pdo` that carries `Rows`, the report by default, beside a
// dictionary of one object.
template<auto const& Rows = report_rows>
consteval std::string check_report(od_pdo const& pdo = report_tpdo)
{
  auto const mapped = pdo_of<Rows>(pdo);
  od_mapped_pdo const pdos[] = {mapped};
  od_row<ctx> const rows[] = {
      {{0x2000, 0x01}, labelled("a.c"), od_ro<read_value>}};
  return detail::od_check(rows, areas, group_a, {.pdos = pdos});
}

// Checks `report_tpdo`, carrying the report, with what `change` changes.
consteval std::string check_changed(auto change)
{
  return check_report(changed(change));
}

static_assert(check_report().empty());
static_assert(check_report<setpoint_rows>(setpoint_rpdo).empty());

// A PDO is named as a segment, which a host joins to the names of its
// signals, and named as no group and no class.
static_assert(check_changed([](od_pdo& pdo) { pdo.name = "2nd"; })
              == "od: TPDO2 2nd: the name must be a letter followed by "
                 "letters, digits or underscores");
static_assert(check_changed([](od_pdo& pdo) { pdo.name = ""; })
              == "od: TPDO2: the name must be a letter followed by letters, "
                 "digits or underscores");
static_assert(check_changed([](od_pdo& pdo) { pdo.name = "a"; })
              == "od: TPDO2 a: named as a group");
static_assert(check_changed([](od_pdo& pdo) { pdo.name = "watch"; })
              == "od: TPDO2 watch: named as a class");

// Its label is that of an object.
static_assert(check_changed([](od_pdo& pdo) { pdo.label = {"Report", ""}; })
              == "od: TPDO2 report: the label must not be empty in any "
                 "language");
static_assert(
    check_changed([](od_pdo& pdo) { pdo.label = {"\"Report\"", "Отчёт"}; })
    == "od: TPDO2 report: the label must be UTF-8 without quotation marks, "
       "backslashes or control characters");

// Its number is from 1 to 512, and of the predefined form from 1 to 4.
static_assert(check_changed([](od_pdo& pdo) { pdo.number = 0; })
              == "od: TPDO0 report: the number must be from 1 to 512");
static_assert(check_changed([](od_pdo& pdo) {
                pdo.number = 513;
                pdo.cob_id = pdo_id::custom(0x405);
              })
              == "od: TPDO513 report: the number must be from 1 to 512");
static_assert(check_changed([](od_pdo& pdo) {
                pdo.number = 512;
                pdo.cob_id = pdo_id::custom(0x405);
              }).empty());
static_assert(check_changed([](od_pdo& pdo) { pdo.number = 4; }).empty());
static_assert(check_changed([](od_pdo& pdo) { pdo.number = 5; })
              == "od: TPDO5 report: the predefined COB-ID exists only for "
                 "PDOs 1 to 4");

// A custom COB-ID has 11 bits and is not restricted.
consteval std::string check_cob_id(id_t id)
{
  return check_changed([&](od_pdo& pdo) {
    pdo.number = 5;
    pdo.cob_id = pdo_id::custom(id);
  });
}

static_assert(check_cob_id(0x000)
              == "od: TPDO5 report: the COB-ID must be from 001h to 7FFh");
static_assert(check_cob_id(0x800)
              == "od: TPDO5 report: the COB-ID must be from 001h to 7FFh");
static_assert(check_cob_id(0x001)
              == "od: TPDO5 report: the COB-ID 001h is restricted");
static_assert(check_cob_id(0x080)
              == "od: TPDO5 report: the COB-ID 080h is restricted");
static_assert(check_cob_id(0x0FF)
              == "od: TPDO5 report: the COB-ID 0FFh is restricted");
static_assert(check_cob_id(0x100)
              == "od: TPDO5 report: the COB-ID 100h is restricted");
static_assert(check_cob_id(0x180)
              == "od: TPDO5 report: the COB-ID 180h is restricted");
static_assert(check_cob_id(0x581)
              == "od: TPDO5 report: the COB-ID 581h is restricted");
static_assert(check_cob_id(0x67F)
              == "od: TPDO5 report: the COB-ID 67Fh is restricted");
static_assert(check_cob_id(0x6E0)
              == "od: TPDO5 report: the COB-ID 6E0h is restricted");
static_assert(check_cob_id(0x7FF)
              == "od: TPDO5 report: the COB-ID 7FFh is restricted");
static_assert(check_cob_id(0x181).empty());
static_assert(check_cob_id(0x580).empty());
static_assert(check_cob_id(0x680).empty());
static_assert(check_cob_id(0x700).empty());
static_assert(!is_restricted_cob_id(0x6DF) && is_restricted_cob_id(0x701));

// A TPDO has a period above 0 and no timeout; an RPDO no period and a
// timeout of 0 or more.
static_assert(check_changed([](od_pdo& pdo) { pdo.period = 0ms; })
              == "od: TPDO2 report: the period must be above 0");
static_assert(check_changed([](od_pdo& pdo) { pdo.period = -1ms; })
              == "od: TPDO2 report: the period must be above 0");
static_assert(check_changed([](od_pdo& pdo) { pdo.timeout = 100ms; })
              == "od: TPDO2 report: a TPDO has no timeout");

consteval std::string check_rpdo(auto change)
{
  od_pdo pdo = setpoint_rpdo;
  change(pdo);
  return check_report<setpoint_rows>(pdo);
}

static_assert(check_rpdo([](od_pdo& pdo) { pdo.timeout = 0ms; }).empty());
static_assert(check_rpdo([](od_pdo& pdo) { pdo.timeout = -1ms; })
              == "od: RPDO5 setpoint: the timeout must not be negative");
static_assert(check_rpdo([](od_pdo& pdo) { pdo.period = 10ms; })
              == "od: RPDO5 setpoint: an RPDO has no period");

// The name of a signal is a segment: one with the name of its PDO before it,
// which a host joins to it itself, one that starts with a digit and one with
// a quotation mark are refused.
inline constexpr std::string_view not_a_signal_name =
    "od: TPDO2 report: the signal at bit 16: the name must be a letter "
    "followed by letters, digits or underscores";

// clang-format off
inline constexpr pdo_row<report> prefixed_rows[] = {
{{16, 16}, "report.voltage", {"Voltage", "Напряжение"}, pdo_map<&report::voltage>}};
inline constexpr pdo_row<report> numbered_rows[] = {
{{16, 16}, "2nd", {"Voltage", "Напряжение"},            pdo_map<&report::voltage>}};
inline constexpr pdo_row<report> quoted_name_rows[] = {
{{16, 16}, "\"v\"", {"Voltage", "Напряжение"},          pdo_map<&report::voltage>}};
// clang-format on

static_assert(check_report<prefixed_rows>() == not_a_signal_name);
static_assert(check_report<numbered_rows>() == not_a_signal_name);
static_assert(check_report<quoted_name_rows>() == not_a_signal_name);

// The label of a signal is that of an object, and no two signals of a PDO
// have the same name; signals of two PDOs may, as the PDOs have different
// names.
// clang-format off
inline constexpr pdo_row<report> quoted_label_rows[] = {
{{16, 16}, "voltage", {"Volt\\age", "Напряжение"}, pdo_map<&report::voltage>}};
inline constexpr pdo_row<report> same_name_rows[] = {
{{ 0,  3}, "voltage", {"Gear", "Передача"},        pdo_map<&report::drive_gear>},
{{16, 16}, "voltage", {"Voltage", "Напряжение"},   pdo_map<&report::voltage>}};
// clang-format on

static_assert(check_report<quoted_label_rows>()
              == "od: TPDO2 report: the signal at bit 16: the label must be "
                 "UTF-8 without quotation marks, backslashes or control "
                 "characters");
static_assert(check_report<same_name_rows>()
              == "od: TPDO2 report: the signal at bit 0 and the signal at "
                 "bit 16 have the same name");

// The codec of the voltage with what `change` changes, as one built by hand
// could be: its unit, its step and its table of values are the
// description's to check.
consteval pdo_codec<report> voltage_codec(auto change)
{
  auto codec = pdo_map<&report::voltage>;
  change(codec);
  return codec;
}

inline constexpr std::string_view voltage_name = "voltage";
inline constexpr emb::i18n::text voltage_label = {"Voltage", "Напряжение"};

// clang-format off
inline constexpr pdo_row<report> quoted_unit_rows[] = {
{{16, 16}, voltage_name, voltage_label, voltage_codec([](auto& codec) { codec.unit = {"V", "В\""}; })}};
inline constexpr pdo_row<report> half_unit_rows[] = {
{{16, 16}, voltage_name, voltage_label, voltage_codec([](auto& codec) { codec.unit = {"V", ""}; })}};
inline constexpr pdo_row<report> unreduced_rows[] = {
{{16, 16}, voltage_name, voltage_label, voltage_codec([](auto& codec) { codec.scale_num = 2; codec.scale_den = 20; })}};
inline constexpr pdo_row<report> negative_step_rows[] = {
{{16, 16}, voltage_name, voltage_label, voltage_codec([](auto& codec) { codec.scale_num = -1; })}};
inline constexpr pdo_row<report> zero_step_rows[] = {
{{16, 16}, voltage_name, voltage_label, voltage_codec([](auto& codec) { codec.scale_num = 0; codec.scale_den = 1; })}};
inline constexpr pdo_row<report> misnamed_table_rows[] = {
{{16, 16}, voltage_name, voltage_label, voltage_codec([](auto& codec) { codec.value_table = misnamed; })}};
// clang-format on

static_assert(check_report<quoted_unit_rows>()
              == "od: TPDO2 report: the signal at bit 16: the unit must be "
                 "UTF-8 without quotation marks, backslashes or control "
                 "characters");
static_assert(check_report<half_unit_rows>()
              == "od: TPDO2 report: the signal at bit 16: the unit must not be "
                 "empty in any language");
static_assert(check_report<unreduced_rows>()
              == "od: TPDO2 report: the signal at bit 16: the step must be a "
                 "positive fraction in its lowest terms");
static_assert(check_report<negative_step_rows>()
              == "od: TPDO2 report: the signal at bit 16: the step must be a "
                 "positive fraction in its lowest terms");
static_assert(check_report<zero_step_rows>()
              == "od: TPDO2 report: the signal at bit 16: the step must be a "
                 "positive fraction in its lowest terms");
static_assert(check_report<misnamed_table_rows>()
              == "od: TPDO2 report: the signal at bit 16: value 0: the name "
                 "must be a letter followed by letters, digits or underscores");

// Checks two PDOs, `first` carrying the report and `second` the setpoint, in
// the rows `Setpoint`.
template<auto const& Setpoint = setpoint_rows>
consteval std::string check_pdo_pair(od_pdo const& first, od_pdo const& second)
{
  auto const report_mapped = pdo_of<report_rows>(first);
  auto const setpoint_mapped = pdo_of<Setpoint>(second);
  od_mapped_pdo const pdos[] = {report_mapped, setpoint_mapped};
  od_row<ctx> const rows[] = {
      {{0x2000, 0x01}, labelled("a.c"), od_ro<read_value>}};
  return detail::od_check(rows, areas, group_a, {.pdos = pdos});
}

// `setpoint_rpdo` with what `change` changes.
consteval od_pdo changed_setpoint(auto change)
{
  od_pdo pdo = setpoint_rpdo;
  change(pdo);
  return pdo;
}

// No two PDOs go one way with one number or have the same name; a TPDO and an
// RPDO may have the same number.
static_assert(check_pdo_pair(report_tpdo, setpoint_rpdo).empty());
static_assert(check_pdo_pair(report_tpdo, changed_setpoint([](od_pdo& pdo) {
                               pdo.number = 2;
                               pdo.cob_id = pdo_id::predefined();
                             }))
                  .empty());
static_assert(check_pdo_pair(report_tpdo, changed_setpoint([](od_pdo& pdo) {
                               pdo.direction = pdo_direction::tx;
                               pdo.number = 2;
                               pdo.period = 10ms;
                               pdo.timeout = 0ms;
                             }))
              == "od: TPDO2 report and TPDO2 setpoint have the same number");

static_assert(check_pdo_pair(report_tpdo, changed_setpoint([](od_pdo& pdo) {
                               pdo.name = "report";
                             }))
              == "od: TPDO2 report and RPDO5 report have the same name");

// The setpoint with a signal named as one of the report, which it may be.
// clang-format off
inline constexpr pdo_row<setpoint> voltage_setpoint_rows[] = {
{{8, 16}, "voltage", {"Voltage", "Напряжение"}, pdo_map<&setpoint::level>}};
// clang-format on

static_assert(
    check_pdo_pair<voltage_setpoint_rows>(report_tpdo, setpoint_rpdo).empty());

// No two PDOs share a COB-ID: a custom one is no other's custom one, and lies
// in the predefined form of no PDO, of either direction, for any node-ID.
consteval std::string check_custom(id_t id)
{
  return check_pdo_pair(report_tpdo, changed_setpoint([&](od_pdo& pdo) {
                          pdo.cob_id = pdo_id::custom(id);
                        }));
}

static_assert(check_custom(0x281)
              == "od: RPDO5 setpoint and TPDO2 report share COB-ID 281h on "
                 "node 1");
static_assert(check_custom(0x2FF)
              == "od: RPDO5 setpoint and TPDO2 report share COB-ID 2FFh on "
                 "node 127");
static_assert(check_custom(0x280).empty());
static_assert(check_custom(0x300).empty());
static_assert(check_pdo_pair(changed([](od_pdo& pdo) {
                               pdo.number = 5;
                               pdo.cob_id = pdo_id::custom(0x405);
                             }),
                             setpoint_rpdo)
              == "od: TPDO5 report and RPDO5 setpoint share COB-ID 405h");
static_assert(check_pdo_pair(changed([](od_pdo& pdo) {
                               pdo.number = 5;
                               pdo.cob_id = pdo_id::custom(0x205);
                             }),
                             changed_setpoint([](od_pdo& pdo) {
                               pdo.number = 1;
                               pdo.cob_id = pdo_id::predefined();
                             }))
              == "od: TPDO5 report and RPDO1 setpoint share COB-ID 205h on "
                 "node 5");

// The rules of the dictionary come before those of the PDOs.
consteval std::string check_dictionary_first()
{
  auto const mapped =
      pdo_of<report_rows>(changed([](od_pdo& pdo) { pdo.name = "2nd"; }));
  od_mapped_pdo const pdos[] = {mapped};
  od_row<ctx> const rows[] = {
      {{0x0FFF, 0x01}, labelled("a.c"), od_ro<read_value>}};
  return detail::od_check(rows, areas, group_a, {.pdos = pdos});
}

static_assert(check_dictionary_first()
              == "od: 0FFFh:01 a.c: indices below 1000h are reserved");

} // namespace
