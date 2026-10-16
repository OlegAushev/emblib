#include <cstdint>
#include <expected>
#include <string>
#include <string_view>

#include <emb/can/canopen/od.hpp>
#include <emb/can/canopen/od_dictionary.hpp>
#include <emb/can/canopen/od_handlers.hpp>
#include <emb/units.hpp>

namespace {

using namespace emb::can::canopen;
using enum od_value_type;

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
  return od_value{std::uint8_t{3}};
}

// The members of a record past 00h: a value kept in the context, the
// record's argument and its second argument.
inline constexpr od_field<ctx> record_layout[] = {
    {.access = od_access::const_, .type = uint8, .read = &read_last_subindex},
    {.access = od_access::rw,
     .type = float32,
     .read = &raw_read,
     .write = &raw_write},
    {.access = od_access::ro, .type = uint16, .read = &read_arg},
    {.access = od_access::ro, .type = uint16, .read = &read_arg, .aux = true},
};

consteval od_binding<ctx> record(std::uint16_t arg, std::uint16_t aux)
{
  return {.access = od_access::ro,
          .type = float32,
          .arg = arg,
          .layout = record_layout,
          .aux = aux};
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

// -- Types --

static_assert(od_type_of<bool> == od_value_type::boolean);
static_assert(od_type_of<std::int8_t> == od_value_type::int8);
static_assert(od_type_of<std::int16_t> == od_value_type::int16);
static_assert(od_type_of<std::int32_t> == od_value_type::int32);
static_assert(od_type_of<std::uint8_t> == od_value_type::uint8);
static_assert(od_type_of<std::uint16_t> == od_value_type::uint16);
static_assert(od_type_of<std::uint32_t> == od_value_type::uint32);
static_assert(od_type_of<float> == od_value_type::float32);

static_assert(od_alternative_of(od_value_type::boolean) == 0);
static_assert(od_alternative_of(od_value_type::float32) == 7);
static_assert(od_alternative_of(od_value_type::exec) == 6);
static_assert(od_alternative_of(od_value_type::string) == 6);

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
// first eight bytes, and so do the index and the two arguments of a record.
static_assert(sizeof(od_entry<ctx>) == 8 + 2 * sizeof(void*));
static_assert(sizeof(od_record<ctx>) == 8 + 2 * sizeof(void*));

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

static_assert(od_text_reader<text, ctx>);
static_assert(od_text_reader<word, ctx>);
static_assert(!od_text_reader<read_value, ctx>);

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
consteval od_value_type read_type()
{
  return bind<od_ro<zero<T>>>().type;
}

static_assert(read_type<bool>() == od_value_type::boolean);
static_assert(read_type<std::int8_t>() == od_value_type::int8);
static_assert(read_type<std::int16_t>() == od_value_type::int16);
static_assert(read_type<std::int32_t>() == od_value_type::int32);
static_assert(read_type<std::uint8_t>() == od_value_type::uint8);
static_assert(read_type<std::uint16_t>() == od_value_type::uint16);
static_assert(read_type<std::uint32_t>() == od_value_type::uint32);
static_assert(read_type<float>() == od_value_type::float32);
static_assert(read_type<emb::units::hz_f32>() == od_value_type::float32);

static_assert(bind<od_ro<read_checked>>().type == od_value_type::float32);

static_assert(bind<od_ro<read_value>>().access == od_access::ro);
static_assert(bind<od_ro<read_value>>().read
              == &detail::od_read_thunk<read_value, ctx>);
static_assert(bind<od_ro<read_value>>().write == nullptr);
static_assert(bind<od_const<read_u8>>().access == od_access::const_);
static_assert(bind<od_const<read_u8>>().type == od_value_type::uint8);

static_assert(bind<od_wo<write_checked>>().access == od_access::wo);
static_assert(bind<od_wo<write_checked>>().type == od_value_type::int16);
static_assert(bind<od_wo<write_checked>>().read == nullptr);
static_assert(bind<od_wo<write_checked>>().write
              == &detail::od_write_thunk<write_checked, ctx>);
static_assert(bind<od_wo<write_freq>>().type == od_value_type::float32);

static_assert(bind<od_rw<read_freq, write_value>>().access == od_access::rw);
static_assert(bind<od_rw<read_freq, write_value>>().type
              == od_value_type::float32);
static_assert(bind<od_rw<read_freq, write_value>>().read
              == &detail::od_read_thunk<read_freq, ctx>);
static_assert(bind<od_rw<read_freq, write_value>>().write
              == &detail::od_write_thunk<write_value, ctx>);

static_assert(bind<od_exec<command>>().access == od_access::wo);
static_assert(bind<od_exec<command>>().type == od_value_type::exec);
static_assert(bind<od_text<text>>().access == od_access::const_);
static_assert(bind<od_text<text>>().type == od_value_type::string);

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

// A text is read 4 bytes at a time, little-endian, with zeros past its end;
// the word that holds the terminating NUL is the last.
consteval bool test_text()
{
  ctx c;
  auto const read = bind<od_text<text>>().read;
  if (read(c, 0) != od_read_result{std::uint32_t{0x64636261}}) return false;
  if (read(c, 1) != od_read_result{std::uint32_t{0x00676665}}) return false;
  if (read(c, 2) != od_read_result{std::uint32_t{0}}) return false;

  auto const whole_word = bind<od_text<word>>().read;
  if (whole_word(c, 0) != od_read_result{std::uint32_t{0x64636261}}) {
    return false;
  }
  return whole_word(c, 1) == od_read_result{std::uint32_t{0}};
}

static_assert(test_text());

// -- Dictionary --

consteval bool test_dictionary()
{
  static constexpr od_binding<ctx> by_hand{.access = od_access::rw,
                                           .type = od_value_type::float32,
                                           .read = &raw_read,
                                           .write = &raw_write,
                                           .arg = 7};
  // clang-format off
  static constexpr od_row<ctx> rows[] = {
      {{0x5000, 0x02}, "watch",  "b", "value",   "",   float32, od_ro<read_value>},
      {{0x2000, 0x01}, "ctl",    "a", "command", "",   exec,    od_exec<command>},
      {{0x5000, 0x01}, "watch",  "b", "checked", "",   float32, od_ro<read_checked>},
      {{0x1008, 0x00}, "info",   "a", "text",    "",   string,  od_text<text>},
      {{0x3000, 0x01}, "config", "a", "freq",    "Hz", float32, od_rw<read_freq, write_value>},
      {{0x3000, 0x02}, "config", "a", "raw",     "",   float32, by_hand},
  };
  // clang-format on

  auto const dictionary = make_dictionary<rows>();
  od_view<ctx> const view{dictionary};

  auto const entries = view.entries();
  if (entries.size() != 6) return false;
  for (auto i = 1uz; i < entries.size(); ++i) {
    if (!(entries[i - 1].key() < entries[i].key())) return false;
  }
  if (entries.front().key() != od_key{0x1008, 0x00}) return false;
  if (entries.back().key() != od_key{0x5000, 0x02}) return false;

  if (!view.records().empty()) return false;

  // An index that rows serve lacks only the subindex; any other, the object.
  auto const no_subindex = std::unexpected(sdo_abort_code::subindex_not_found);
  auto const no_object = std::unexpected(sdo_abort_code::object_not_found);
  if (view.find({0x5000, 0x03}) != no_subindex) return false;
  if (view.find({0x5000, 0x00}) != no_subindex) return false;
  if (view.find({0x3000, 0x00}) != no_subindex) return false;
  if (view.find({0x0FFF, 0x00}) != no_object) return false;
  if (view.find({0x4000, 0x01}) != no_object) return false;
  if (view.find({0xFFFF, 0xFF}) != no_object) return false;

  auto const raw = view.find({0x3000, 0x02});
  if (!raw || (raw->arg != 7)) return false;

  ctx c;
  auto const freq = view.find({0x3000, 0x01});
  if (!freq || (freq->access != od_access::rw)) return false;
  if (!freq->write(c, freq->arg, od_value{4.5f})) return false;
  if (freq->read(c, freq->arg) != od_read_result{4.5f}) return false;
  return raw->read(c, raw->arg) == od_read_result{4.5f};
}

static_assert(test_dictionary());

// A record serves every subindex of its index through its layout, which
// records share: each member gets the argument its field names.
consteval bool test_records()
{
  // clang-format off
  static constexpr od_row<ctx> rows[] = {
      {{0x4001, 0x00}, "config", "r", "second", "", float32, record(2, 0x4101)},
      {{0x5000, 0x01}, "watch",  "b", "value",  "", float32, od_ro<read_value>},
      {{0x4000, 0x00}, "config", "r", "first",  "", float32, record(1, 0x4100)},
  };
  // clang-format on

  auto const dictionary = make_dictionary<rows>();
  od_view<ctx> const view{dictionary};

  if (view.entries().size() != 1) return false;
  auto const records = view.records();
  if ((records.size() != 2) || (records[0].index != 0x4000)) return false;
  if ((records[1].arg != 2) || (records[1].aux != 0x4101)) return false;

  ctx c;
  auto const last = view.find({0x4000, 0x00});
  if (!last || (last->access != od_access::const_) || (last->type != uint8)) {
    return false;
  }
  if (last->read(c, last->arg) != od_read_result{std::uint8_t{3}}) return false;

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
  if (read_member({0x4000, 0x03}) != od_read_result{std::uint16_t{0x4100}}) {
    return false;
  }
  if (read_member({0x4001, 0x03}) != od_read_result{std::uint16_t{0x4101}}) {
    return false;
  }

  if (view.find({0x4000, 0x04})
      != std::unexpected(sdo_abort_code::subindex_not_found)) {
    return false;
  }
  return view.find({0x4002, 0x00})
      == std::unexpected(sdo_abort_code::object_not_found);
}

static_assert(test_records());

// -- Checks --

// Builds the binding of an object from its parts, for the checks below.
consteval od_binding<ctx>
raw(od_access access, od_value_type type, bool reader, bool writer)
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
      {{0x2000, 0x01}, "ctl",   "a", "command", "", exec,    od_exec<command>},
      {{0x5000, 0x01}, "watch", "a", "value",   "", float32, od_ro<read_value>},
  };
  // clang-format on
  return detail::od_check(rows);
}

static_assert(check_valid().empty());

consteval std::string check_reserved_index()
{
  // clang-format off
  static constexpr od_row<ctx> rows[] = {
      {{0x0FFF, 0x01}, "a", "b", "c", "", float32, od_ro<read_value>},
  };
  // clang-format on
  return detail::od_check(rows);
}

static_assert(check_reserved_index()
              == "od: 0FFFh:01 a/b/c: indices below 1000h are reserved");

consteval std::string check_empty_name()
{
  // clang-format off
  static constexpr od_row<ctx> rows[] = {
      {{0x2000, 0x01}, "a", "", "c", "", float32, od_ro<read_value>},
  };
  // clang-format on
  return detail::od_check(rows);
}

static_assert(check_empty_name()
              == "od: 2000h:01 a//c: category, subcategory and name must not "
                 "be empty");

consteval std::string check_one(od_binding<ctx> binding)
{
  od_row<ctx> const rows[] = {
      {{0x2000, 0x01}, "a", "b", "c", "", binding.type, binding}};
  return detail::od_check(rows);
}

static_assert(
    check_one(raw(od_access::ro, od_value_type::float32, false, false))
    == "od: 2000h:01 a/b/c: readable but has no reader");
static_assert(check_one(raw(od_access::wo, od_value_type::float32, true, true))
              == "od: 2000h:01 a/b/c: write-only but has a reader");
static_assert(check_one(raw(od_access::rw, od_value_type::float32, true, false))
              == "od: 2000h:01 a/b/c: writable but has no writer");
static_assert(check_one(raw(od_access::ro, od_value_type::float32, true, true))
              == "od: 2000h:01 a/b/c: read-only but has a writer");
static_assert(check_one(raw(od_access::rw, od_value_type::exec, true, true))
              == "od: 2000h:01 a/b/c: an exec object must be wo");
static_assert(check_one(raw(od_access::rw, od_value_type::string, true, true))
              == "od: 2000h:01 a/b/c: a string object must not be writable");

// The type a row states must be the type its binding serves.
consteval std::string check_declared(od_value_type declared,
                                     od_binding<ctx> binding)
{
  od_row<ctx> const rows[] = {
      {{0x2000, 0x01}, "a", "b", "c", "", declared, binding}};
  return detail::od_check(rows);
}

static_assert(check_declared(float32, od_ro<read_freq>).empty());
static_assert(check_declared(float32, od_ro<read_u8>)
              == "od: 2000h:01 a/b/c: declared float32, bound as uint8");
static_assert(check_declared(exec, od_text<text>)
              == "od: 2000h:01 a/b/c: declared exec, bound as string");

inline constexpr std::string_view items[] = {"x", "y"};
inline constexpr bool all_exposed[] = {true, true};
inline constexpr bool y_hidden[] = {true, false};
inline constexpr od_catalog catalog{"item", items, all_exposed};
inline constexpr od_catalog hiding_catalog{"item", items, y_hidden};

consteval od_binding<ctx> item(std::uint16_t arg, od_catalog const& from)
{
  return {.access = od_access::ro,
          .type = od_value_type::float32,
          .read = &raw_read,
          .arg = arg,
          .catalog = &from};
}

static_assert(check_one(item(2, catalog))
              == "od: 2000h:01 a/b/c: arg is outside its catalog");

// A record is checked field by field, at the row that names it.
consteval std::string check_record(od_binding<ctx> binding)
{
  od_row<ctx> const rows[] = {
      {{0x2000, 0x00}, "a", "b", "c", "", binding.type, binding}};
  return detail::od_check(rows);
}

inline constexpr od_field<ctx> unread_layout[] = {
    {.access = od_access::const_, .type = uint8, .read = &read_last_subindex},
    {.access = od_access::ro, .type = float32},
};

static_assert(check_record(record(1, 0)).empty());
static_assert(check_one(record(1, 0))
              == "od: 2000h:01 a/b/c: a record's row must have subindex 00");
static_assert(check_record({.access = od_access::ro,
                            .type = float32,
                            .read = &raw_read,
                            .layout = record_layout})
              == "od: 2000h:00 a/b/c: a record has no handlers of its own");
static_assert(check_record({.access = od_access::ro,
                            .type = float32,
                            .layout = unread_layout})
              == "od: 2000h:00 a/b/c: member 01h readable but has no reader");

consteval std::string check_two(od_row<ctx> first, od_row<ctx> second)
{
  od_row<ctx> const rows[] = {first, second};
  return detail::od_check(rows);
}

static_assert(
    check_two({{0x2000, 0x01}, "a", "b", "c", "", float32, od_ro<read_value>},
              {{0x2000, 0x01}, "a", "b", "d", "", uint8, od_ro<read_u8>})
    == "od: 2000h:01 a/b/c and 2000h:01 a/b/d have the same key");
static_assert(
    check_two({{0x2000, 0x01}, "a", "b", "c", "", float32, od_ro<read_value>},
              {{0x2000, 0x02}, "a", "b", "c", "", uint8, od_ro<read_u8>})
    == "od: 2000h:01 a/b/c and 2000h:02 a/b/c have the same name");
static_assert(
    check_two({{0x2000, 0x00}, "a", "b", "c", "", float32, record(1, 0)},
              {{0x2000, 0x05}, "a", "b", "d", "", uint8, od_ro<read_u8>})
    == "od: 2000h:00 a/b/c and 2000h:05 a/b/d share the index of a record");
static_assert(
    check_two({{0x2000, 0x00}, "a", "b", "c", "", float32, record(1, 0)},
              {{0x2001, 0x00}, "a", "b", "d", "", float32, record(1, 0)})
        .empty());
// Checks two objects that differ in key and name, 2000h:01 a/b/c and
// 2000h:02 a/b/d.
consteval std::string check_pair(od_binding<ctx> first, od_binding<ctx> second)
{
  return check_two({{0x2000, 0x01}, "a", "b", "c", "", first.type, first},
                   {{0x2000, 0x02}, "a", "b", "d", "", second.type, second});
}

consteval od_binding<ctx> reading(std::uint16_t arg)
{
  return {.access = od_access::ro,
          .type = od_value_type::float32,
          .read = &raw_read,
          .arg = arg};
}

static_assert(check_pair(od_ro<read_value>, od_ro<read_value>)
              == "od: 2000h:01 a/b/c and 2000h:02 a/b/d are read by the same "
                 "handler");
static_assert(check_pair(od_wo<write_value>, od_wo<write_value>)
              == "od: 2000h:01 a/b/c and 2000h:02 a/b/d are written by the "
                 "same handler");
// The same handler with different arguments serves different objects.
static_assert(check_pair(reading(0), reading(1)).empty());

static_assert(check_pair(item(0, catalog), item(0, catalog))
              == "od: item 'x' has two rows: 2000h:01 a/b/c and 2000h:02 "
                 "a/b/d");
static_assert(check_one(item(0, catalog)) == "od: item 'y' has no row");
static_assert(check_pair(item(0, catalog), item(1, catalog)).empty());
static_assert(check_pair(item(0, hiding_catalog), item(1, hiding_catalog))
              == "od: item 'y' is not exposed but has a row: 2000h:02 a/b/d");
static_assert(check_one(item(0, hiding_catalog)).empty());

// A builder that has reported its error silences the checks.
consteval od_binding<ctx> diagnosed()
{
  return {.access = od_access::ro,
          .type = od_value_type::uint32,
          .diagnosed = true};
}

static_assert(
    check_two({{0x0FFF, 0x01}, "a", "b", "c", "", float32, od_ro<read_value>},
              {{0x2000, 0x02}, "a", "b", "d", "", uint32, diagnosed()})
        .empty());

} // namespace
