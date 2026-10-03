#include <algorithm>
#include <array>
#include <span>

#include <emb/settings/record.hpp>
#include <emb/units.hpp>

namespace {

using namespace emb;
using namespace emb::settings;

using rpm = units::rpm_f32;

inline constexpr std::uint32_t magic = 0x53544553u; // "SETS"

inline constexpr auto schema =
    make_schema(param("motor.p",
                      std::int32_t{11},
                      {.min = std::int32_t{1}, .max = std::int32_t{64}}),
                param("motor.R", 0.0014f, {.min = 0.0f, .max = 1.0f}),
                param("drive.phase_swap", false),
                param("drive.runout_speed",
                      rpm{100.0f},
                      {.min = rpm{0.0f}, .max = rpm{5000.0f}}));

// A later firmware: one parameter added, one retyped, one dropped.
inline constexpr auto next_schema =
    make_schema(param("motor.p",
                      std::int32_t{11},
                      {.min = std::int32_t{1}, .max = std::int32_t{64}}),
                param("motor.R", 0.0014f, {.min = 0.0f, .max = 1.0f}),
                param("drive.phase_swap", std::int32_t{0}),
                param("hall.enabled", true));

// A later firmware, two parameters richer.
inline constexpr auto richer_schema =
    make_schema(param("motor.p",
                      std::int32_t{11},
                      {.min = std::int32_t{1}, .max = std::int32_t{64}}),
                param("motor.R", 0.0014f, {.min = 0.0f, .max = 1.0f}),
                param("drive.phase_swap", false),
                param("drive.runout_speed",
                      rpm{100.0f},
                      {.min = rpm{0.0f}, .max = rpm{5000.0f}}),
                param("hall.enabled", true),
                param("hall.poll_num", std::int32_t{4}));

using record_buffer = std::array<std::byte, record_size(schema.count)>;
// A slot as a store reads it: the record, then erased bytes up to its end.
using slot_buffer = std::array<std::byte, 128>;

constexpr slot_buffer erased_slot()
{
  slot_buffer slot{};
  slot.fill(std::byte{0xFF});
  return slot;
}

constexpr void restamp_crc(std::span<std::byte> record)
{
  auto const end = record.size() - 4;
  emb::settings::detail::put_u32(
      record,
      end,
      emb::settings::detail::crc32(record.first(end)));
}

// -- Layout --

static_assert(record_body_size(0) == 16);
static_assert(record_size(0) == 24);
static_assert(record_size(4) == 24 + 32);
static_assert(record_size(57) == 480);

// Both halves of a record are multiples of eight, so a store can commit the
// footer separately on any medium whose write granularity divides eight.
static_assert(record_body_size(57) % 8 == 0);
static_assert(record_footer_size % 8 == 0);

static_assert(seq_newer(2, 1));
static_assert(!seq_newer(1, 2));
static_assert(!seq_newer(1, 1));
// The counter may wrap without the rollover looking ancient.
static_assert(seq_newer(0, 0xFFFFFFFFu));
static_assert(!seq_newer(0xFFFFFFFFu, 0));

// -- Round trip --

consteval bool test_round_trip()
{
  image<schema> written;
  if (!written.set<"motor.p">(std::int32_t{4})) return false;
  if (!written.set<"motor.R">(0.05f)) return false;
  if (!written.set<"drive.phase_swap">(true)) return false;
  if (!written.set<"drive.runout_speed">(rpm{250.0f})) return false;

  record_buffer buffer{};
  if (encode_record(buffer, written, magic, 7) != buffer.size()) return false;

  image<schema> read;
  auto const report = decode_record(buffer, magic, read);

  if (!report.valid) return false;
  if (!report.schema_matched) return false;
  if (report.seq != 7) return false;
  if (report.stored != 4 || report.loaded != 4) return false;
  if (report.unknown != 0 || report.rejected != 0) return false;
  if (report.missing != 0) return false;

  if (read.get<"motor.p">() != 4) return false;
  if (read.get<"motor.R">() != 0.05f) return false;
  if (read.get<"drive.phase_swap">() != true) return false;
  if (read.get<"drive.runout_speed">() != rpm{250.0f}) return false;

  return true;
}

consteval bool test_header_scan()
{
  image<schema> values;
  record_buffer buffer{};
  encode_record(buffer, values, magic, 42);

  auto const header = decode_header(buffer, magic);
  if (!header) return false;
  if (header->count != schema.count) return false;
  if (header->seq != 42) return false;
  if (header->format != record_format) return false;
  if (header->schema_id != schema_id<schema>()) return false;

  // Another section's slot is not this section's record.
  if (decode_header(buffer, magic + 1).has_value()) return false;
  // Nor is a buffer too short to hold a header.
  if (decode_header(std::span{buffer}.first(8), magic).has_value()) {
    return false;
  }

  return true;
}

// -- Integrity --

consteval bool test_a_flipped_bit_is_caught_anywhere()
{
  image<schema> values;
  record_buffer clean{};
  encode_record(clean, values, magic, 1);

  for (auto i = 0uz; i < clean.size(); ++i) {
    auto buffer = clean;
    buffer[i] ^= std::byte{0x01};

    image<schema> read;
    if (decode_record(buffer, magic, read).valid) return false;
  }
  return true;
}

consteval bool test_an_interrupted_write_is_not_a_record()
{
  image<schema> values;
  record_buffer buffer{};
  encode_record(buffer, values, magic, 1);

  // Power lost before the footer: the body is there, the commit is not.
  for (auto i = buffer.size() - record_footer_size; i < buffer.size(); ++i)
    buffer[i] = std::byte{0xFF};

  image<schema> read;
  if (decode_record(buffer, magic, read).valid) return false;

  // Truncated in the middle of the cells.
  image<schema> other;
  auto const cut = std::span<std::byte const>{buffer}.first(buffer.size() - 8);
  if (decode_record(cut, magic, other).valid) return false;

  return true;
}

consteval bool test_a_failed_decode_leaves_the_image_alone()
{
  image<schema> values;
  if (!values.set<"motor.p">(std::int32_t{7})) return false;

  record_buffer buffer{}; // never written: all zeroes, no magic
  auto const report = decode_record(buffer, magic, values);

  if (report.valid) return false;
  if (values.get<"motor.p">() != 7) return false;

  return true;
}

// -- Whole records --

// A store sorts its slots by `check_record` alone, so a whole record must
// come back with its header, whether the span ends with the record or runs
// on to the end of the slot.
consteval bool test_check_record_returns_the_header()
{
  image<schema> values;
  record_buffer record{};
  encode_record(record, values, magic, 42);

  auto slot = erased_slot();
  std::ranges::copy(record, slot.begin());

  for (auto const header :
       {check_record(record, magic), check_record(slot, magic)}) {
    if (!header) return false;
    if (header->magic != magic) return false;
    if (header->format != record_format) return false;
    if (header->count != schema.count) return false;
    if (header->seq != 42) return false;
    if (header->schema_id != schema_id<schema>()) return false;
  }

  return true;
}

// A save cut short leaves a prefix of its record over erased bytes. However
// far it got, the slot is no record until the last byte of the record that
// differs from 0xFF lands: a scan that took the debris for one would count
// it as the newest. This record ends in another byte, so every prefix short
// of the whole record is debris.
consteval bool test_no_prefix_of_a_record_is_whole()
{
  image<schema> values;
  record_buffer record{};
  encode_record(record, values, magic, 7);
  if (record.back() == std::byte{0xFF}) return false;

  for (auto length = 1uz; length <= record.size(); ++length) {
    auto slot = erased_slot();
    std::copy_n(record.begin(), length, slot.begin());
    if (check_record(slot, magic).has_value() != (length == record.size())) {
      return false;
    }
  }

  return true;
}

// The footer commits a record, and the CRC does not stand in for it: a
// wrong magic there fails the record under a CRC that matches its bytes.
// A wrong CRC fails it under the right magic.
consteval bool test_a_damaged_footer_is_not_whole()
{
  image<schema> values;
  record_buffer clean{};
  encode_record(clean, values, magic, 7);

  auto wrong_magic = clean;
  emb::settings::detail::put_u32(wrong_magic,
                                 wrong_magic.size() - 8,
                                 magic + 1);
  restamp_crc(wrong_magic);
  if (check_record(wrong_magic, magic).has_value()) return false;

  auto wrong_crc = clean;
  wrong_crc[wrong_crc.size() - 1] ^= std::byte{0x01};
  if (check_record(wrong_crc, magic).has_value()) return false;

  return true;
}

// A record of another section, or of another version of the layout, is not
// a record of this one, however whole it is.
consteval bool test_a_foreign_record_is_not_whole()
{
  image<schema> values;

  record_buffer other_section{};
  encode_record(other_section, values, magic + 1, 7);
  if (check_record(other_section, magic).has_value()) return false;
  if (!check_record(other_section, magic + 1).has_value()) return false;

  record_buffer other_format{};
  encode_record(other_format, values, magic, 7);
  emb::settings::detail::put_u16(other_format,
                                 4,
                                 static_cast<std::uint16_t>(record_format + 1));
  restamp_crc(other_format);
  if (check_record(other_format, magic).has_value()) return false;

  return true;
}

// The header decides how far the record reaches. A span that ends a byte
// short of it, or a count whose top bit rotted, puts the footer past the
// span: the record is not whole, and nothing past the span is read.
consteval bool test_a_count_past_the_span_is_not_whole()
{
  image<schema> values;
  record_buffer record{};
  encode_record(record, values, magic, 7);

  auto const short_by_one =
      std::span<std::byte const>{record}.first(record.size() - 1);
  if (check_record(short_by_one, magic).has_value()) return false;

  auto slot = erased_slot();
  std::ranges::copy(record, slot.begin());
  emb::settings::detail::put_u16(
      slot,
      6,
      static_cast<std::uint16_t>(schema.count | 0x8000u));
  if (check_record(slot, magic).has_value()) return false;

  return true;
}

// A firmware with more parameters writes a longer record than this one
// does. Where the slot holds it, it is whole: the check goes by the count
// the record declares, not by the schema of the reader.
consteval bool test_a_richer_record_is_whole_where_it_fits()
{
  image<richer_schema> values;
  auto slot = erased_slot();
  if (encode_record(slot, values, magic, 9)
      != record_size(richer_schema.count)) {
    return false;
  }

  auto const header = check_record(slot, magic);
  if (!header) return false;
  if (header->count != richer_schema.count) return false;
  if (header->seq != 9) return false;
  if (header->schema_id != schema_id<richer_schema>()) return false;

  return true;
}

// -- Migration --

consteval bool test_a_record_from_another_schema()
{
  image<next_schema> written;
  if (!written.set<"motor.p">(std::int32_t{4})) return false;
  if (!written.set<"hall.enabled">(false)) return false;

  std::array<std::byte, record_size(next_schema.count)> buffer{};
  encode_record(buffer, written, magic, 3);

  image<schema> read;
  auto const report = decode_record(buffer, magic, read);

  if (!report.valid) return false;
  // Same magic and format, different parameters.
  if (report.schema_matched) return false;

  // motor.p and motor.R still match by identifier; hall.enabled is not a
  // parameter here, and drive.phase_swap was retyped, so its identifier no
  // longer matches either.
  if (report.stored != 4) return false;
  if (report.loaded != 2) return false;
  if (report.unknown != 2) return false;

  // What the record did not carry keeps the default of this firmware.
  if (report.missing != 2) return false;
  if (read.get<"motor.p">() != 4) return false;
  if (read.get<"drive.phase_swap">() != false) return false;
  if (read.get<"drive.runout_speed">() != rpm{100.0f}) return false;

  return true;
}

consteval bool test_a_value_outside_todays_range_is_refused()
{
  image<schema> values;
  record_buffer buffer{};
  encode_record(buffer, values, magic, 1);

  // Stand in for a record written when the range was wider: patch the cell
  // and re-stamp the crc, so the record is whole but the value is not one
  // this firmware accepts.
  constexpr auto at =
      record_header_size + (*schema.index_of("motor.p") * record_cell_size);
  // Qualified: emb::detail and emb::settings::detail are both in scope
  // through the using-directives above.
  namespace bytes = settings::detail;
  bytes::put_u32(buffer, at + 4, to_raw(std::int32_t{1000}));
  bytes::put_u32(buffer,
                 buffer.size() - 4,
                 bytes::crc32(std::span<std::byte const>{buffer}.first(
                     buffer.size() - 4)));

  image<schema> read;
  auto const report = decode_record(buffer, magic, read);

  if (!report.valid) return false;
  if (report.rejected != 1) return false;
  if (report.loaded != schema.count - 1) return false;
  // Carried and refused, not absent: the two read differently.
  if (report.missing != 0) return false;
  // Refused, so the parameter comes up with its default rather than with a
  // value the control code was never meant to see.
  if (read.get<"motor.p">() != 11) return false;

  return true;
}

static_assert(test_round_trip());
static_assert(test_header_scan());
static_assert(test_a_flipped_bit_is_caught_anywhere());
static_assert(test_an_interrupted_write_is_not_a_record());
static_assert(test_a_failed_decode_leaves_the_image_alone());
static_assert(test_check_record_returns_the_header());
static_assert(test_no_prefix_of_a_record_is_whole());
static_assert(test_a_damaged_footer_is_not_whole());
static_assert(test_a_foreign_record_is_not_whole());
static_assert(test_a_count_past_the_span_is_not_whole());
static_assert(test_a_richer_record_is_whole_where_it_fits());
static_assert(test_a_record_from_another_schema());
static_assert(test_a_value_outside_todays_range_is_refused());

} // namespace
