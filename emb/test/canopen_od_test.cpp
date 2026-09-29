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
  c.value = v.value();
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

constexpr od_write_result restore_value(ctx& c, std::uint16_t arg)
{
  c.value = static_cast<float>(arg);
  return {};
}

constexpr od_write_result restore_other(ctx&, std::uint16_t)
{
  return {};
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

// Two indices, a subindex and four one-byte fields share the first word.
static_assert(sizeof(od_entry<ctx>) == 8 + 2 * sizeof(void*));

// -- Handler shapes --

static_assert(od_reader<read_value, ctx>);
static_assert(od_reader<read_freq, ctx>);
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

static_assert(bind<od_restore_default>().access == od_access::wo);
static_assert(bind<od_restore_default>().type == od_value_type::exec);
static_assert(bind<od_restore_default>().read == nullptr);
static_assert(bind<od_restore_default>().write == nullptr);

static_assert(!bind<od_ro<read_value>>().diagnosed);
static_assert(bind<od_ro<read_value>>().restore == nullptr);
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
  static constexpr od_binding<ctx> restorable{.access = od_access::rw,
                                              .type = od_value_type::float32,
                                              .read = &raw_read,
                                              .write = &raw_write,
                                              .arg = 7,
                                              .restore = &restore_value};
  // clang-format off
  static constexpr od_row<ctx> rows[] = {
      {{0x5000, 0x02}, "watch",  "b", "value",   "",   od_ro<read_value>},
      {{0x2000, 0x01}, "ctl",    "a", "command", "",   od_exec<command>},
      {{0x5000, 0x01}, "watch",  "b", "checked", "",   od_ro<read_checked>},
      {{0x1008, 0x00}, "info",   "a", "text",    "",   od_text<text>},
      {{0x3000, 0x01}, "config", "a", "freq",    "Hz", od_rw<read_freq, write_value>},
      {{0x3000, 0x02}, "config", "a", "raw",     "",   restorable},
      {{0x1011, 0x04}, "ctl",    "a", "restore", "",   od_restore_default},
  };
  // clang-format on

  auto const dictionary = make_dictionary<rows>();
  od_view<ctx> const view{dictionary};

  auto const entries = view.entries();
  if (entries.size() != 7) return false;
  for (auto i = 1uz; i < entries.size(); ++i) {
    if (!(entries[i - 1].key() < entries[i].key())) return false;
  }
  if (entries.front().key() != od_key{0x1008, 0x00}) return false;
  if (entries.back().key() != od_key{0x5000, 0x02}) return false;

  if (view.find({0x5000, 0x03}) != nullptr) return false;
  if (view.find({0x0FFF, 0x00}) != nullptr) return false;
  if (view.find({0xFFFF, 0xFF}) != nullptr) return false;

  auto const* raw = view.find({0x3000, 0x02});
  if ((raw == nullptr) || !raw->restorable || (raw->arg != 7)) return false;
  if (view.find({0x3000, 0x01})->restorable) return false;
  if (view.restore() != &restore_value) return false;

  ctx c;
  auto const* freq = view.find({0x3000, 0x01});
  if (freq->access != od_access::rw) return false;
  if (!freq->write(c, freq->arg, od_value{4.5f})) return false;
  if (freq->read(c, freq->arg) != od_read_result{4.5f}) return false;

  if (!view.restore()(c, raw->arg)) return false;
  if (c.value != 7.f) return false;

  auto const* restore = view.find(od_restore_default_key);
  return (restore != nullptr)
      && (restore->access == od_access::wo)
      && (restore->read == nullptr)
      && (restore->write == nullptr);
}

static_assert(test_dictionary());

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
      {{0x1011, 0x04}, "ctl",   "a", "restore", "", od_restore_default},
      {{0x2000, 0x01}, "ctl",   "a", "command", "", od_exec<command>},
      {{0x5000, 0x01}, "watch", "a", "value",   "", od_ro<read_value>},
  };
  // clang-format on
  return detail::od_check(rows);
}

static_assert(check_valid().empty());

consteval std::string check_reserved_index()
{
  // clang-format off
  static constexpr od_row<ctx> rows[] = {
      {{0x0FFF, 0x01}, "a", "b", "c", "", od_ro<read_value>},
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
      {{0x2000, 0x01}, "a", "", "c", "", od_ro<read_value>},
  };
  // clang-format on
  return detail::od_check(rows);
}

static_assert(check_empty_name()
              == "od: 2000h:01 a//c: category, subcategory and name must not "
                 "be empty");

consteval std::string check_one(od_binding<ctx> binding)
{
  od_row<ctx> const rows[] = {{{0x2000, 0x01}, "a", "b", "c", "", binding}};
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
static_assert(check_one({.access = od_access::ro,
                         .type = od_value_type::float32,
                         .read = &raw_read,
                         .restore = &restore_value})
              == "od: 2000h:01 a/b/c: restorable but not writable");

consteval std::string check_restore_key()
{
  // clang-format off
  static constexpr od_row<ctx> rows[] = {
      {{0x1011, 0x04}, "a", "b", "c", "", od_exec<command>},
  };
  // clang-format on
  return detail::od_check(rows);
}

static_assert(check_restore_key()
              == "od: 1011h:04 a/b/c: the server serves this key; bind it "
                 "with od_restore_default");

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

consteval std::string check_two(od_row<ctx> first, od_row<ctx> second)
{
  od_row<ctx> const rows[] = {first, second};
  return detail::od_check(rows);
}

static_assert(check_two({{0x2000, 0x01}, "a", "b", "c", "", od_ro<read_value>},
                        {{0x2000, 0x01}, "a", "b", "d", "", od_ro<read_u8>})
              == "od: 2000h:01 a/b/c and 2000h:01 a/b/d have the same key");
static_assert(check_two({{0x2000, 0x01}, "a", "b", "c", "", od_ro<read_value>},
                        {{0x2000, 0x02}, "a", "b", "c", "", od_ro<read_u8>})
              == "od: 2000h:01 a/b/c and 2000h:02 a/b/c have the same name");
// Checks two objects that differ in key and name, 2000h:01 a/b/c and
// 2000h:02 a/b/d.
consteval std::string check_pair(od_binding<ctx> first, od_binding<ctx> second)
{
  return check_two({{0x2000, 0x01}, "a", "b", "c", "", first},
                   {{0x2000, 0x02}, "a", "b", "d", "", second});
}

consteval od_binding<ctx> restoring(od_restore_fn<ctx> restore,
                                    std::uint16_t arg)
{
  return {.access = od_access::rw,
          .type = od_value_type::float32,
          .read = &raw_read,
          .write = &raw_write,
          .arg = arg,
          .restore = restore};
}

consteval od_binding<ctx> reading(std::uint16_t arg)
{
  return {.access = od_access::ro,
          .type = od_value_type::float32,
          .read = &raw_read,
          .arg = arg};
}

static_assert(check_pair(restoring(&restore_value, 0),
                         restoring(&restore_other, 1))
              == "od: 2000h:01 a/b/c and 2000h:02 a/b/d restore through "
                 "different functions");
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

static_assert(check_two({{0x0FFF, 0x01}, "a", "b", "c", "", od_ro<read_value>},
                        {{0x2000, 0x02}, "a", "b", "d", "", diagnosed()})
                  .empty());

} // namespace
