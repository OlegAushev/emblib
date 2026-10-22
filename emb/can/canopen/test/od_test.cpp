#include <array>
#include <cstdint>
#include <expected>
#include <span>
#include <string>
#include <string_view>

#include <emb/can/canopen/od.hpp>
#include <emb/can/canopen/od_description.hpp>
#include <emb/can/canopen/od_dictionary.hpp>
#include <emb/can/canopen/od_handlers.hpp>
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

// The labels of the members of `record_layout`; the value is the record's.
inline constexpr od_field_label record_labels[] = {
    {.name = "highest_subindex",
     .title = {"Highest subindex", "Наибольший подындекс"}},
    {.name = "value", .title = {"Value", "Значение"}, .inherits = true},
    {.name = "arg", .title = {"Argument", "Аргумент"}},
};

consteval od_binding<ctx> record(std::uint16_t arg)
{
  return {.access = od_access::ro,
          .type = float32,
          .arg = arg,
          .layout = record_layout,
          .field_labels = record_labels};
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

// The unit of an object is the symbol of the type of its value, if the type
// names one: of what the reader returns or the writer takes, of the elements
// of an array.
static_assert(bind<od_ro<read_freq>>().unit == "Hz");
static_assert(bind<od_ro<read_value>>().unit.empty());
static_assert(bind<od_const<zero<emb::units::edeg_f32>>>().unit == "\u00B0el");
static_assert(bind<od_wo<write_freq>>().unit == "Hz");
static_assert(bind<od_ro_array<read_rates>>().unit == "Hz");
static_assert(bind<od_ro_array<samples>>().unit.empty());
static_assert(bind<od_exec<command>>().unit.empty());
static_assert(bind<od_string<text>>().unit.empty());

// The reader and the writer of an rw object agree on the unit if one of them
// names none; the unit is the one named.
static_assert(bind<od_rw<read_freq, write_value>>().unit == "Hz");
static_assert(bind<od_rw<read_value, write_freq>>().unit == "Hz");
static_assert(bind<od_rw<read_freq, write_freq>>().unit == "Hz");
static_assert(bind<od_rw<read_value, write_value>>().unit.empty());
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

// The label of an object of the tables below, titled after its path.
consteval od_label titled(std::string_view path)
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
      {{0x5000, 0x02}, titled("b.value"),                          od_ro<read_value>},
      {{0x2100, 0x01}, titled("a.command"),                        od_exec<command>},
      {{0x5000, 0x01}, titled("b.checked"),                        od_ro<read_checked>},
      {{0x3000, 0x01}, {"a.freq", {"Frequency", "Частота"}},       od_rw<read_freq, write_value>},
      {{0x3000, 0x02}, titled("a.raw"),                            by_hand},
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
      {{0x4001, 0x00}, titled("a.second"), record(2)},
      {{0x5000, 0x01}, titled("b.value"),  od_ro<read_value>},
      {{0x4000, 0x00}, titled("a.first"),  record(1)},
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
      {{0x2001, 0x00}, titled("a.word"),    od_string<word>},
      {{0x5001, 0x00}, titled("b.samples"), od_ro_array<samples>},
      {{0x5000, 0x01}, titled("b.value"),   od_ro<read_value>},
      {{0x2000, 0x00}, titled("a.text"),    od_string<text>},
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
    {{0x5000, 0x01}, {"b.value", {"Value", "Значение"}, "V"}, od_ro<read_value>},
    {{0x2100, 0x01}, {"a.save", {"Save", "Сохранить"}},       od_exec<command, od_save_signature>},
    {{0x2000, 0x00}, {"a.text", {"Text", "Текст"}},           od_string<text>},
    {{0x4000, 0x00}, {"a.first", {"First", "Первый"}, "A"},   record(1)},
    {{0x5001, 0x00}, {"b.rates", {"Rates", "Частоты"}},       od_ro_array<read_rates>},
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
    "{\"type\":\"group\",\"name\":\"a\",\"title\":\"Group A\","
    "\"title_ru\":\"Группа А\"}\n"
    "{\"type\":\"group\",\"name\":\"b\",\"title\":\"Group B\","
    "\"title_ru\":\"Группа Б\"}\n"
    "{\"type\":\"field\",\"layout\":0,\"sub\":\"0x00\","
    "\"name\":\"highest_subindex\",\"title\":\"Highest subindex\","
    "\"title_ru\":\"Наибольший подындекс\",\"datatype\":\"uint8\","
    "\"access\":\"const\"}\n"
    "{\"type\":\"field\",\"layout\":0,\"sub\":\"0x01\",\"name\":\"value\","
    "\"title\":\"Value\",\"title_ru\":\"Значение\",\"access\":\"rw\","
    "\"inherits\":true}\n"
    "{\"type\":\"field\",\"layout\":0,\"sub\":\"0x02\",\"name\":\"arg\","
    "\"title\":\"Argument\",\"title_ru\":\"Аргумент\",\"datatype\":\"uint16\","
    "\"access\":\"ro\"}\n"
    "{\"type\":\"array\",\"index\":\"0x2000\",\"path\":\"a.text\","
    "\"title\":\"Text\",\"title_ru\":\"Текст\",\"class\":\"about\","
    "\"datatype\":\"uint8\",\"access\":\"const\",\"text\":true}\n"
    "{\"type\":\"object\",\"index\":\"0x2100\",\"sub\":\"0x01\","
    "\"path\":\"a.save\",\"title\":\"Save\",\"title_ru\":\"Сохранить\","
    "\"class\":\"control\",\"datatype\":\"uint32\",\"access\":\"wo\","
    "\"command\":true,\"signature\":\"0x65766173\"}\n"
    "{\"type\":\"record\",\"index\":\"0x4000\",\"path\":\"a.first\","
    "\"title\":\"First\",\"title_ru\":\"Первый\",\"class\":\"parameters\","
    "\"datatype\":\"float32\",\"unit\":\"A\",\"layout\":0}\n"
    "{\"type\":\"object\",\"index\":\"0x5000\",\"sub\":\"0x01\","
    "\"path\":\"b.value\",\"title\":\"Value\",\"title_ru\":\"Значение\","
    "\"class\":\"watch\",\"datatype\":\"float32\",\"access\":\"ro\","
    "\"unit\":\"V\"}\n"
    "{\"type\":\"array\",\"index\":\"0x5001\",\"path\":\"b.rates\","
    "\"title\":\"Rates\",\"title_ru\":\"Частоты\",\"class\":\"watch\","
    "\"datatype\":\"float32\",\"access\":\"ro\",\"unit\":\"Hz\"}\n";

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

// The fingerprint follows the paths, the classes and the titles: a row
// renamed, retitled or in an area of another class changes it.
inline constexpr od_area reclassed[] = {
    {0x2000, 0x20FF, od_class::about},
    {0x2100, 0x2FFF, od_class::control},
    {0x3000, 0x4FFF, od_class::watch},
    {0x5000, 0x5FFF, od_class::watch},
};

inline constexpr od_row<ctx> retitled[] = {
    described[0],
    described[1],
    described[2],
    {{0x4000, 0x00}, {"a.first", {"First", "Первая"}, "A"}, record(1)},
    described[4]};

static_assert(
    make_dictionary<described, reclassed, described_groups>().fingerprint()
    != fnv1a(described_body));
static_assert(make_dictionary<retitled, areas, described_groups>().fingerprint()
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

inline constexpr od_field_label byte_labels[] = {
    {.name = "highest_subindex",
     .title = {"Highest subindex", "Наибольший подындекс"}},
    {.name = "value", .title = {"Value", "Значение"}, .inherits = true},
    {.name = "save",
     .title = {"Save", "Сохранить"},
     .command = true,
     .signature = od_save_signature},
};

// clang-format off
inline constexpr od_row<ctx> layouts[] = {
    {{0x4002, 0x00}, titled("a.byte"),   {.access = od_access::ro, .type = uint8, .layout = byte_layout, .field_labels = byte_labels}},
    {{0x4001, 0x00}, titled("a.other"),  {.access = od_access::ro, .type = float32, .arg = 2, .layout = other_layout, .field_labels = record_labels}},
    {{0x4000, 0x00}, titled("a.first"),  record(1)},
    {{0x5000, 0x01}, titled("b.value"),  od_ro<read_value>},
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
                            "\"name\":\"highest_subindex\",\"title\":"
                            "\"Highest subindex\",\"title_ru\":\"Наибольший "
                            "подындекс\",\"datatype\":\"uint8\",\"access\":"
                            "\"const\"}\n"));
static_assert(as_view(described_layouts)
                  .contains("{\"type\":\"field\",\"layout\":1,\"sub\":\"0x01\","
                            "\"name\":\"value\",\"title\":\"Value\","
                            "\"title_ru\":\"Значение\",\"access\":\"const\","
                            "\"inherits\":true}\n"));
static_assert(
    as_view(described_layouts)
        .contains("{\"type\":\"field\",\"layout\":1,\"sub\":\"0x02\","
                  "\"name\":\"save\",\"title\":\"Save\","
                  "\"title_ru\":\"Сохранить\",\"datatype\":\"uint32\","
                  "\"access\":\"wo\",\"command\":true,"
                  "\"signature\":\"0x65766173\"}\n"));
static_assert(as_view(described_layouts)
                  .contains("{\"type\":\"record\",\"index\":\"0x4002\","
                            "\"path\":\"a.byte\",\"title\":\"a.byte\","
                            "\"title_ru\":\"a.byte\",\"class\":\"parameters\","
                            "\"datatype\":\"uint8\",\"layout\":1}\n"));

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
inline constexpr od_value_label policies[] = {
    {0, "on_command", {"On command", "По команде"}},
    {1, "on_restart", {"On restart", "После перезапуска"}},
};

inline constexpr od_value_label modes[] = {
    {0, "off", {"Off", "Выкл."}},
    {1, "on", {"On", "Вкл."}},
};

inline constexpr od_value_label levels[] = {
    {-1, "low", {"Low", "Низкий"}},
    {300, "high", {"High", "Высокий"}},
};

// The values of `modes` in an array of their own: the same table for a host.
inline constexpr od_value_label same_modes[] = {modes[0], modes[1]};

// A record of a named uint8 whose member 02h, a policy, names values of its
// own.
inline constexpr od_field<ctx> mode_layout[] = {
    {.access = od_access::const_, .type = uint8, .read = &read_last_subindex},
    {.access = od_access::ro, .type = uint8, .read = &read_byte},
    {.access = od_access::const_, .type = uint8, .read = &read_byte},
};

inline constexpr od_field_label mode_labels[] = {
    {.name = "highest_subindex",
     .title = {"Highest subindex", "Наибольший подындекс"}},
    {.name = "value", .title = {"Value", "Значение"}, .inherits = true},
    {.name = "policy",
     .title = {"Policy", "Политика"},
     .value_labels = policies},
};

consteval od_binding<ctx>
mode_record(std::span<od_field_label const> labels = mode_labels)
{
  return {.access = od_access::ro,
          .type = uint8,
          .layout = mode_layout,
          .field_labels = labels,
          .value_labels = same_modes};
}

inline constexpr od_binding<ctx> level_object{.access = od_access::ro,
                                              .type = int16,
                                              .read = &read_level,
                                              .value_labels = levels};

inline constexpr od_binding<ctx> mode_array{.access = od_access::ro,
                                            .type = uint8,
                                            .read = &read_byte,
                                            .count = &read_last_subindex,
                                            .value_labels = modes};

// The rows of named values, not in the order of their keys.
// clang-format off
inline constexpr od_row<ctx> valued[] = {
    {{0x5002, 0x01}, titled("b.level"), level_object},
    {{0x5003, 0x00}, titled("b.modes"), mode_array},
    {{0x4003, 0x00}, titled("a.mode"),  mode_record()},
};
// clang-format on

inline constexpr od_row<ctx> revalued[] = {valued[2], valued[0], valued[1]};

// The body of the description of `valued`: the labels of the values after
// the groups, by table and by value, the tables numbered as the members of
// the layouts and then the rows by key first name them, each once, though
// `modes` and `same_modes` are two arrays; then the members and the rows,
// each that has a table referring to it.
inline constexpr std::string_view valued_body =
    "{\"type\":\"group\",\"name\":\"a\",\"title\":\"Group A\","
    "\"title_ru\":\"Группа А\"}\n"
    "{\"type\":\"group\",\"name\":\"b\",\"title\":\"Group B\","
    "\"title_ru\":\"Группа Б\"}\n"
    "{\"type\":\"value_label\",\"value_table\":0,\"value\":0,"
    "\"name\":\"on_command\",\"title\":\"On command\","
    "\"title_ru\":\"По команде\"}\n"
    "{\"type\":\"value_label\",\"value_table\":0,\"value\":1,"
    "\"name\":\"on_restart\",\"title\":\"On restart\","
    "\"title_ru\":\"После перезапуска\"}\n"
    "{\"type\":\"value_label\",\"value_table\":1,\"value\":0,\"name\":\"off\","
    "\"title\":\"Off\",\"title_ru\":\"Выкл.\"}\n"
    "{\"type\":\"value_label\",\"value_table\":1,\"value\":1,\"name\":\"on\","
    "\"title\":\"On\",\"title_ru\":\"Вкл.\"}\n"
    "{\"type\":\"value_label\",\"value_table\":2,\"value\":-1,\"name\":\"low\","
    "\"title\":\"Low\",\"title_ru\":\"Низкий\"}\n"
    "{\"type\":\"value_label\",\"value_table\":2,\"value\":300,"
    "\"name\":\"high\",\"title\":\"High\",\"title_ru\":\"Высокий\"}\n"
    "{\"type\":\"field\",\"layout\":0,\"sub\":\"0x00\","
    "\"name\":\"highest_subindex\",\"title\":\"Highest subindex\","
    "\"title_ru\":\"Наибольший подындекс\",\"datatype\":\"uint8\","
    "\"access\":\"const\"}\n"
    "{\"type\":\"field\",\"layout\":0,\"sub\":\"0x01\",\"name\":\"value\","
    "\"title\":\"Value\",\"title_ru\":\"Значение\",\"access\":\"ro\","
    "\"inherits\":true}\n"
    "{\"type\":\"field\",\"layout\":0,\"sub\":\"0x02\",\"name\":\"policy\","
    "\"title\":\"Policy\",\"title_ru\":\"Политика\",\"datatype\":\"uint8\","
    "\"access\":\"const\",\"value_table\":0}\n"
    "{\"type\":\"record\",\"index\":\"0x4003\",\"path\":\"a.mode\","
    "\"title\":\"a.mode\",\"title_ru\":\"a.mode\",\"class\":\"parameters\","
    "\"datatype\":\"uint8\",\"value_table\":1,\"layout\":0}\n"
    "{\"type\":\"object\",\"index\":\"0x5002\",\"sub\":\"0x01\","
    "\"path\":\"b.level\",\"title\":\"b.level\",\"title_ru\":\"b.level\","
    "\"class\":\"watch\",\"datatype\":\"int16\",\"access\":\"ro\","
    "\"value_table\":2}\n"
    "{\"type\":\"array\",\"index\":\"0x5003\",\"path\":\"b.modes\","
    "\"title\":\"b.modes\",\"title_ru\":\"b.modes\",\"class\":\"watch\","
    "\"datatype\":\"uint8\",\"access\":\"ro\",\"value_table\":1}\n";

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

// Layouts that differ in the table of a member are two; those whose labels
// are two arrays of the same content are one.
inline constexpr od_field_label same_mode_labels[] = {mode_labels[0],
                                                      mode_labels[1],
                                                      mode_labels[2]};

inline constexpr od_field_label other_mode_labels[] = {
    mode_labels[0],
    mode_labels[1],
    {.name = "policy", .title = {"Policy", "Политика"}, .value_labels = modes},
};

// clang-format off
inline constexpr od_row<ctx> policy_layouts[] = {
    {{0x4003, 0x00}, titled("a.mode"),  mode_record()},
    {{0x4004, 0x00}, titled("a.same"),  mode_record(same_mode_labels)},
    {{0x4005, 0x00}, titled("a.other"), mode_record(other_mode_labels)},
    {{0x5002, 0x01}, titled("b.level"), level_object},
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
static_assert(occurrences(as_view(described_policies),
                          "\"type\":\"value_label\"")
              == 6);
static_assert(as_view(described_policies)
                  .contains("\"name\":\"policy\",\"title\":\"Policy\","
                            "\"title_ru\":\"Политика\",\"datatype\":\"uint8\","
                            "\"access\":\"const\",\"value_table\":1}\n"));
static_assert(occurrences(as_view(described_policies), "\"value_table\":1,")
              == 5);

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
      {{0x2100, 0x01}, titled("a.command"), od_exec<command>},
      {{0x5000, 0x01}, titled("a.value"),   od_ro<read_value>},
  };
  // clang-format on
  return detail::od_check(rows, areas, group_a);
}

static_assert(check_valid().empty());

consteval std::string check_reserved_index()
{
  // clang-format off
  static constexpr od_row<ctx> rows[] = {
      {{0x0FFF, 0x01}, titled("a.c"), od_ro<read_value>},
  };
  // clang-format on
  return detail::od_check(rows, areas, group_a);
}

static_assert(check_reserved_index()
              == "od: 0FFFh:01 a.c: indices below 1000h are reserved");

consteval std::string check_one(od_binding<ctx> binding)
{
  od_row<ctx> const rows[] = {{{0x2000, 0x01}, titled("a.c"), binding}};
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
      {{0x6000, 0x01}, titled("a.c"), od_ro<read_value>},
  };
  // clang-format on
  return detail::od_check(rows, areas, group_a);
}

static_assert(check_outside() == "od: 6000h:01 a.c: the index lies in no area");

// -- Checks: labels --

// Checks the label `label` of an object at 2000h:01.
consteval std::string check_label(od_label label)
{
  od_row<ctx> const rows[] = {{{0x2000, 0x01}, label, od_ro<read_value>}};
  return detail::od_check(rows, areas, group_a);
}

inline constexpr std::string_view not_a_path =
    ": the path must be a group and a name, each a letter followed by "
    "letters, digits or underscores";

static_assert(check_label(titled("a.c")).empty());
static_assert(check_label(titled("a.Vdc_2")).empty());
static_assert(check_label(titled("a"))
              == "od: 2000h:01 a" + std::string{not_a_path});
static_assert(check_label(titled("a.c.d"))
              == "od: 2000h:01 a.c.d" + std::string{not_a_path});
static_assert(check_label(titled("a.1c"))
              == "od: 2000h:01 a.1c" + std::string{not_a_path});
static_assert(check_label(titled("a._c"))
              == "od: 2000h:01 a._c" + std::string{not_a_path});
static_assert(check_label(titled("a.c-d"))
              == "od: 2000h:01 a.c-d" + std::string{not_a_path});
static_assert(check_label(titled(".c"))
              == "od: 2000h:01 .c" + std::string{not_a_path});
static_assert(check_label(titled("a/c"))
              == "od: 2000h:01 a/c" + std::string{not_a_path});
static_assert(check_label({}) == "od: 2000h:01" + std::string{not_a_path});
static_assert(check_label(titled("x.c"))
              == "od: 2000h:01 x.c: group 'x' is not declared");

// Checks the title `en`, `ru` of an object at 2000h:01.
consteval std::string check_title(std::string_view en, std::string_view ru)
{
  return check_label({"a.c", {en, ru}});
}

inline constexpr std::string_view not_verbatim =
    "od: 2000h:01 a.c: the title must be UTF-8 without quotation marks, "
    "backslashes or control characters";

static_assert(check_title("Motor temperature, °C", "Температура, °C").empty());
static_assert(check_title("Resistance, Ω", "Сопротивление, Ω").empty());
static_assert(check_title("Smile \U0001F600", "Улыбка \U0001F600").empty());
static_assert(check_title("C", "")
              == "od: 2000h:01 a.c: the title must not be empty in any "
                 "language");
static_assert(check_title("", "Ц")
              == "od: 2000h:01 a.c: the title must not be empty in any "
                 "language");
static_assert(check_title("The \"c\"", "Ц") == not_verbatim);
static_assert(check_title("C", "Ц\\") == not_verbatim);
static_assert(check_title("C\n", "Ц") == not_verbatim);
static_assert(check_title("C\x7F", "Ц") == not_verbatim);
// U+0085, a control character of Latin-1.
static_assert(check_title("C\xC2\x85", "Ц") == not_verbatim);
// UTF-8 cut short, a stray continuation byte, a byte that never starts a
// character, the overlong form of '/', a surrogate and a code point past
// U+10FFFF.
static_assert(check_title("C\xC3", "Ц") == not_verbatim);
static_assert(check_title("C\x80", "Ц") == not_verbatim);
static_assert(check_title("C\xFF", "Ц") == not_verbatim);
static_assert(check_title("C\xC0\xAF", "Ц") == not_verbatim);
static_assert(check_title("C\xED\xA0\x80", "Ц") == not_verbatim);
static_assert(check_title("C\xF4\x90\x80\x80", "Ц") == not_verbatim);

// A binding that gives its object's label, as that of a settings parameter
// does.
consteval od_binding<ctx> named()
{
  return {.access = od_access::ro,
          .type = od_data_type::float32,
          .read = &raw_read,
          .label = {"a.named", {"Named", "Названный"}}};
}

static_assert(check_one(named())
              == "od: 2000h:01 a.named: the row has a label, "
                 "but its binding gives one");

consteval std::string check_named(od_label label)
{
  od_row<ctx> const rows[] = {{{0x2000, 0x01}, label, named()}};
  return detail::od_check(rows, areas, group_a);
}

static_assert(check_named({}).empty());
static_assert(check_named({"", {}, "V"})
              == "od: 2000h:01 a.named: the row has a label, but its binding "
                 "gives one");

// -- Checks: units --

// Checks the object at 2000h:00, of the path "a.c" and the unit `unit`,
// bound by `binding`.
consteval std::string check_unit(std::string_view unit, od_binding<ctx> binding)
{
  od_row<ctx> const rows[] = {
      {{0x2000, 0x00}, {"a.c", {"C", "Ц"}, unit}, binding}};
  return detail::od_check(rows, areas, group_a);
}

// A unit for a value whose type names none, or in place of the symbol of the
// type, which the label must not repeat.
static_assert(check_unit("V", od_ro<read_value>).empty());
static_assert(check_unit("rpm", od_ro<read_freq>).empty());
static_assert(check_unit("", od_ro<read_freq>).empty());
static_assert(check_unit("Hz", od_ro<read_freq>)
              == "od: 2000h:00 a.c: the unit 'Hz' repeats the symbol of the "
                 "value's type");
static_assert(check_unit("Hz", od_ro_array<read_rates>)
              == "od: 2000h:00 a.c: the unit 'Hz' repeats the symbol of the "
                 "value's type");
// No unit for a command, an array of characters or a boolean.
static_assert(check_unit("V", od_exec<command>)
              == "od: 2000h:00 a.c: a command has no unit");
static_assert(check_unit("V", od_string<text>)
              == "od: 2000h:00 a.c: an array of characters has no unit");
static_assert(check_unit("V", od_ro<zero<bool>>)
              == "od: 2000h:00 a.c: a boolean has no unit");
static_assert(check_unit("\"V\"", od_ro<read_value>)
              == "od: 2000h:00 a.c: the unit must be UTF-8 without quotation "
                 "marks, backslashes or control characters");
static_assert(check_unit("\u03A9", od_ro<read_value>).empty());

// -- Checks: areas and groups --

// Checks the object at 2000h:01, of the path "a.c", against `in`.
template<std::size_t A>
consteval std::string check_in(od_area const (&in)[A])
{
  od_row<ctx> const rows[] = {
      {{0x2000, 0x01}, titled("a.c"), od_ro<read_value>}};
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
      {{0x2000, 0x01}, titled("a.c"), od_ro<read_value>}};
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
static_assert(check_group({"a", {"A again", "Снова А"}})
              == "od: group 'a': declared twice");
static_assert(check_group({"b", {"B", ""}})
              == "od: group 'b': the title must not be empty in any "
                 "language");
static_assert(check_group({"b", {"B\t", "Б"}})
              == "od: group 'b': the title must be UTF-8 without quotation "
                 "marks, backslashes or control characters");
static_assert(check_group({"b", {"B", "Б"}}) == "od: group 'b': has no row");

// A record is checked field by field, at the row that names it.
consteval std::string check_record(od_binding<ctx> binding)
{
  od_row<ctx> const rows[] = {{{0x2000, 0x00}, titled("a.c"), binding}};
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

// The members of a record are labelled: one label for each, named, titled,
// a member that inherits the value of the record's type, and a signature for
// a command only.
consteval std::string check_labels(std::span<od_field_label const> labels,
                                   std::string_view unit = {})
{
  od_row<ctx> const rows[] = {{{0x2000, 0x00},
                               {"a.c", {"C", "Ц"}, unit},
                               {.access = od_access::ro,
                                .type = float32,
                                .layout = record_layout,
                                .field_labels = labels}}};
  return detail::od_check(rows, areas, group_a);
}

// The labels of `record_layout`, the member at `at` replaced with `label`.
consteval std::array<od_field_label, 3> relabelled(std::size_t at,
                                                   od_field_label label)
{
  std::array<od_field_label, 3> out{record_labels[0],
                                    record_labels[1],
                                    record_labels[2]};
  out[at] = label;
  return out;
}

static_assert(check_labels(record_labels).empty());
static_assert(check_labels(std::span{record_labels}.first(2))
              == "od: 2000h:00 a.c: the record has 3 members but labels 2");
static_assert(check_labels(relabelled(1, {.name = "2nd", .title = {"V", "З"}}))
              == "od: 2000h:00 a.c: member 01h: the name must be a letter "
                 "followed by letters, digits or underscores");
static_assert(check_labels(relabelled(2,
                                      {.name = "highest_subindex",
                                       .title = {"A", "А"}}))
              == "od: 2000h:00 a.c: members 00h and 02h have the same name");
static_assert(check_labels(relabelled(1, {.name = "value", .title = {"V", ""}}))
              == "od: 2000h:00 a.c: member 01h: the title must not be empty in "
                 "any language");
static_assert(
    check_labels(relabelled(1, {.name = "value", .title = {"V\"", "З"}}))
    == "od: 2000h:00 a.c: member 01h: the title must be UTF-8 without "
       "quotation marks, backslashes or control characters");
static_assert(check_labels(relabelled(2,
                                      {.name = "arg",
                                       .title = {"A", "А"},
                                       .signature = od_save_signature}))
              == "od: 2000h:00 a.c: member 02h has a signature but is no "
                 "command");
static_assert(check_labels(relabelled(
                  2,
                  {.name = "arg", .title = {"A", "А"}, .inherits = true}))
              == "od: 2000h:00 a.c: member 02h inherits the record's type, "
                 "float32, but is uint16");
// A record whose value no member holds has no unit, nor does its line state
// a type.
static_assert(
    check_labels(relabelled(1, {.name = "value", .title = {"V", "З"}}), "A")
    == "od: 2000h:00 a.c: a record without a member that inherits "
       "its value has no unit");

inline constexpr std::array<od_field_label, 3> plain_labels =
    relabelled(1, {.name = "value", .title = {"Value", "Значение"}});

inline constexpr od_row<ctx> plain[] = {
    {{0x4000, 0x00},
     titled("a.plain"),
     {.access = od_access::ro,
      .type = float32,
      .layout = record_layout,
      .field_labels = plain_labels}},
};

static_assert(
    as_view(make_description<plain, areas, group_a, "Test device">())
        .ends_with("{\"type\":\"record\",\"index\":\"0x4000\","
                   "\"path\":\"a.plain\",\"title\":\"a.plain\","
                   "\"title_ru\":\"a.plain\",\"class\":\"parameters\","
                   "\"layout\":0}\n"));

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

static_assert(check_two({{0x2000, 0x01}, titled("a.c"), od_ro<read_value>},
                        {{0x2000, 0x01}, titled("a.d"), od_ro<read_u8>})
              == "od: 2000h:01 a.c and 2000h:01 a.d have the same key");
static_assert(check_two({{0x2000, 0x01}, titled("a.c"), od_ro<read_value>},
                        {{0x2000, 0x02}, titled("a.c"), od_ro<read_u8>})
              == "od: 2000h:01 a.c and 2000h:02 a.c have the same path");
static_assert(
    check_two({{0x2000, 0x00}, titled("a.c"), record(1)},
              {{0x2000, 0x05}, titled("a.d"), od_ro<read_u8>})
    == "od: 2000h:00 a.c and 2000h:05 a.d share the index of a record");
static_assert(check_two({{0x2000, 0x00}, titled("a.c"), record(1)},
                        {{0x2001, 0x00}, titled("a.d"), record(1)})
                  .empty());
static_assert(
    check_two({{0x2000, 0x00}, titled("a.c"), od_string<text>},
              {{0x2000, 0x05}, titled("a.d"), od_ro<read_u8>})
    == "od: 2000h:00 a.c and 2000h:05 a.d share the index of an array");
static_assert(
    check_two({{0x2000, 0x05}, titled("a.d"), od_ro<read_u8>},
              {{0x2000, 0x00}, titled("a.c"), od_ro<read_value>})
    == "od: 2000h:05 a.d and 2000h:00 a.c share an index whose subindex 00 "
       "the dictionary serves");
// Checks two objects that differ in key and path, 2000h:01 a.c and
// 2000h:02 a.d.
consteval std::string check_pair(od_binding<ctx> first, od_binding<ctx> second)
{
  return check_two({{0x2000, 0x01}, titled("a.c"), first},
                   {{0x2000, 0x02}, titled("a.d"), second});
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

static_assert(check_two({{0x0FFF, 0x01}, titled("a.c"), od_ro<read_value>},
                        {{0x2000, 0x02}, titled("a.d"), diagnosed()})
                  .empty());

// -- Checks: values --

// Checks the object at 2000h:00, of the path "a.c" and the unit `unit`, bound
// by `binding` and its values named by `values`.
consteval std::string check_table(od_binding<ctx> binding,
                                  std::span<od_value_label const> values,
                                  std::string_view unit = {})
{
  binding.value_labels = values;
  od_row<ctx> const rows[] = {
      {{0x2000, 0x00}, {"a.c", {"C", "Ц"}, unit}, binding}};
  return detail::od_check(rows, areas, group_a);
}

consteval od_binding<ctx> raw_reader(od_data_type type)
{
  return raw(od_access::ro, type, true, false);
}

inline constexpr od_value_label bytes_past[] = {
    {0, "zero", {"Zero", "Ноль"}},
    {256, "past", {"Past a byte", "За байтом"}},
};

inline constexpr od_value_label signed_bytes[] = {
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
static_assert(check_table(raw_reader(uint8), modes, "V")
              == "od: 2000h:00 a.c: values that a table names have no unit");

// The labels of `byte_layout` whose member 01h holds no value of the record.
inline constexpr od_field_label byte_plain_labels[] = {
    byte_labels[0],
    {.name = "value", .title = {"Value", "Значение"}},
    byte_labels[2],
};

// A record names the values of its members that inherit its value, and so
// has a table only if one does.
static_assert(check_table({.access = od_access::ro,
                           .type = uint8,
                           .layout = byte_layout,
                           .field_labels = byte_plain_labels},
                          modes)
              == "od: 2000h:00 a.c: a record without a member that inherits "
                 "its value has no table of values");

// A member names its values, unless it inherits those of the record, in a
// type that takes a table, each a value of the type.
static_assert(check_labels(relabelled(2,
                                      {.name = "arg",
                                       .title = {"A", "А"},
                                       .value_labels = modes}))
                  .empty());
static_assert(check_labels(relabelled(1,
                                      {.name = "value",
                                       .title = {"V", "З"},
                                       .inherits = true,
                                       .value_labels = modes}))
              == "od: 2000h:00 a.c: member 01h inherits the record's values "
                 "and has no table of its own");
static_assert(
    check_labels(relabelled(
        1,
        {.name = "value", .title = {"V", "З"}, .value_labels = modes}))
    == "od: 2000h:00 a.c: member 01h: a float32 has no table of values");
static_assert(check_labels(relabelled(2,
                                      {.name = "arg",
                                       .title = {"A", "А"},
                                       .command = true,
                                       .value_labels = modes}))
              == "od: 2000h:00 a.c: member 02h: a command has no table of "
                 "values");
static_assert(check_labels(relabelled(
                  2,
                  {.name = "arg", .title = {"A", "А"}, .value_labels = levels}))
              == "od: 2000h:00 a.c: member 02h: value -1 does not fit uint16");

// Checks the values `first` and `second` of a uint8 object at 2000h:00.
consteval std::string check_two_values(od_value_label first,
                                       od_value_label second)
{
  od_value_label const values[] = {first, second};
  return check_table(raw_reader(uint8), values);
}

// A table names each value as a segment of a path, titles it, and holds the
// values in increasing order, no two with the same name or with the same
// title in one language: two titles "A (001)" are a copy not corrected. The
// title of a value may be the same in both languages, as an abbreviation is.

static_assert(check_two_values({0, "svpwm", {"SVPWM", "SVPWM"}},
                               {1, "dpwm0", {"DPWM0", "DPWM0"}})
                  .empty());
static_assert(check_two_values({0, "a", {"A", "А"}}, {1, "2nd", {"B", "Б"}})
              == "od: 2000h:00 a.c: value 1: the name must be a letter "
                 "followed by letters, digits or underscores");
static_assert(check_two_values({0, "a", {"A", "А"}}, {1, "b", {"B", ""}})
              == "od: 2000h:00 a.c: value 1: the title must not be empty in "
                 "any language");
static_assert(check_two_values({0, "a", {"A", "А"}}, {1, "b", {"B\"", "Б"}})
              == "od: 2000h:00 a.c: value 1: the title must be UTF-8 without "
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
              == "od: 2000h:00 a.c: values 0 and 1 have the same title in "
                 "English");
static_assert(check_two_values({0, "a", {"A", "Один"}}, {1, "b", {"B", "Один"}})
              == "od: 2000h:00 a.c: values 0 and 1 have the same title in "
                 "Russian");

// A table is checked once, and a broken one is reported at the first row that
// names its values with it, as at the first record whose member does.
inline constexpr od_value_label misnamed[] = {{0, "2nd", {"A", "А"}}};

inline constexpr od_field_label misnamed_labels[] = {
    mode_labels[0],
    mode_labels[1],
    {.name = "policy",
     .title = {"Policy", "Политика"},
     .value_labels = misnamed},
};

consteval std::string check_shared_tables()
{
  // clang-format off
  static constexpr od_row<ctx> rows[] = {
      {{0x5002, 0x01}, titled("a.first"),  {.access = od_access::ro, .type = int16, .read = &read_level, .value_labels = misnamed}},
      {{0x5002, 0x02}, titled("a.second"), {.access = od_access::ro, .type = uint8, .read = &read_byte, .value_labels = misnamed}},
  };
  // clang-format on
  return detail::od_check(rows, areas, group_a);
}

consteval std::string check_shared_member_tables()
{
  // clang-format off
  static constexpr od_row<ctx> rows[] = {
      {{0x4001, 0x00}, titled("a.first"),  mode_record(misnamed_labels)},
      {{0x4002, 0x00}, titled("a.second"), mode_record(misnamed_labels)},
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

} // namespace
