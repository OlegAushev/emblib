#include <array>
#include <span>

#include <emb/settings/record.hpp>
#include <emb/units.hpp>

namespace {

using namespace emb;
using namespace emb::settings;

using rpm = units::rpm_f32;

inline constexpr std::uint32_t magic = 0x53544553u; // "SETS"

inline constexpr auto schema = make_schema(
    param("motor.p",
          std::int32_t{11},
          {.min = std::int32_t{1}, .max = std::int32_t{64}}),
    param("motor.R", 0.0014f, {.min = 0.0f, .max = 1.0f}),
    param("drive.phase_swap", false),
    param("drive.runout_speed",
          rpm{100.0f},
          {.min = rpm{0.0f}, .max = rpm{5000.0f}}));

// A later firmware: one parameter added, one retyped, one dropped.
inline constexpr auto next_schema = make_schema(
    param("motor.p",
          std::int32_t{11},
          {.min = std::int32_t{1}, .max = std::int32_t{64}}),
    param("motor.R", 0.0014f, {.min = 0.0f, .max = 1.0f}),
    param("drive.phase_swap", std::int32_t{0}),
    param("hall.enabled", true));

using record_buffer = std::array<std::byte, record_size(schema.count)>;

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
  constexpr auto at = record_header_size
                    + (schema.index_of("motor.p") * record_cell_size);
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
  if (report.missing != 1) return false;
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
static_assert(test_a_record_from_another_schema());
static_assert(test_a_value_outside_todays_range_is_refused());

} // namespace
