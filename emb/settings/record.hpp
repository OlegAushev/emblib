#pragma once

#include <emb/settings/image.hpp>
#include <emb/settings/schema.hpp>

#include <optional>
#include <span>

#include <cstddef>
#include <cstdint>

namespace emb {
namespace settings {

// Version of the record layout described below, stored in the `format`
// field of every record. `decode_header`, and with it `decode_record`,
// rejects a record of any other version.
//
// A record is the stored form of an image: a header, one cell for each
// parameter of the schema it was written with, and a footer. All fields are
// little-endian, whatever the byte order of the host:
//
//   0x00       4  magic      identifies the section
//   0x04       2  format     version of the layout, `record_format`
//   0x06       2  count      number of cells, N
//   0x08       4  seq        sequence number, compared by `seq_newer`
//   0x0C       4  schema_id  `schema_id` of the writer's schema
//   0x10      8N  cells      N times { u32 id; u32 value }
//   0x10+8N    4  magic      repeated
//   0x14+8N    4  crc32      `detail::crc32` of all bytes before it
//
// Each cell is stored with the identifier of its parameter, and cells are
// matched to parameters by identifier, not by position: adding, removing or
// reordering parameters leaves the format as it is, and a record written
// with another schema still loads cell by cell. The identifier is derived
// from the name and the `value_type` of the parameter, so a parameter that
// is renamed or changes its `value_type` no longer matches its old cell and
// loads with its default value.
//
// The body, i.e. the header and the cells (`record_body_size` bytes), and
// the footer (`record_footer_size` bytes) are both multiples of eight bytes,
// so a medium whose write granularity divides eight can write either one
// alone. A store writes the body first and the footer last, never the other
// way round: the footer commits the record, and until every byte of it holds
// its value the record fails the checks of `decode_record`.
inline constexpr std::uint16_t record_format = 1;

// Sizes, in bytes, of the header of a record, of one cell with its
// identifier, and of the footer.
inline constexpr std::size_t record_header_size = 16;
inline constexpr std::size_t record_cell_size = 8;
inline constexpr std::size_t record_footer_size = 8;

// Returns the size, in bytes, of the body of a record of `count` cells,
// i.e. of its header and cells: the part a store writes before the footer.
constexpr std::size_t record_body_size(std::size_t count)
{
  return record_header_size + count * record_cell_size;
}

// Returns the size, in bytes, of a whole record of `count` cells, i.e.
// `record_body_size(count) + record_footer_size`.
constexpr std::size_t record_size(std::size_t count)
{
  return record_body_size(count) + record_footer_size;
}

// Structure holding the fields of the header of a record, as decoded by
// `decode_header`.
struct record_header {
  std::uint32_t magic;
  std::uint16_t format;
  std::uint16_t count;
  std::uint32_t seq;
  std::uint32_t schema_id;
};

// Checks whether the sequence number `a` is newer than `b`, i.e. whether
// `a - b`, taken modulo 2^32, lies in [1, 2^31). The comparison is modular,
// so the counter may wrap: zero is newer than `0xFFFFFFFF`.
//
// The relation is not an ordering: it is not transitive, and of two numbers
// 2^31 apart neither is newer than the other. It cannot serve as the
// comparator of a sort or of a standard min/max algorithm.
constexpr bool seq_newer(std::uint32_t a, std::uint32_t b)
{
  return static_cast<std::int32_t>(a - b) > 0;
}

// Returns the fingerprint of `Schema` that a record stores in its
// `schema_id` field: the 32-bit FNV-1a hash of the identifiers of the
// parameters in declaration order, each taken as four little-endian bytes.
// A record whose fingerprint differs still loads; `decode_record` reports
// the difference in `load_report::schema_matched`.
template<auto& Schema>
consteval std::uint32_t schema_id()
{
  std::uint32_t h = 0x811C9DC5u;
  for (auto const& p : Schema.parameters)
    for (auto shift = 0; shift < 32; shift += 8) {
      h ^= (p.id >> shift) & 0xFFu;
      h *= 0x01000193u;
    }
  return h;
}

// Structure holding the outcome of `decode_record`. If `valid` is `false`,
// every other member holds its default value.
//
// Each cell of the record counts in exactly one of `loaded`, `unknown` and
// `rejected`. A parameter counts in `missing` only if the record has no cell
// for it: one whose cell was rejected does not.
struct load_report {
  bool valid = false;          // whether a whole record was decoded
  bool schema_matched = false; // stored `schema_id` is the schema's
  std::uint32_t seq = 0;
  std::uint16_t stored = 0;   // cells the record carries
  std::uint16_t loaded = 0;   // cells accepted into the image
  std::uint16_t unknown = 0;  // cells whose identifier is not in the schema
  std::uint16_t rejected = 0; // cells outside the range their descriptor allows
  std::uint16_t missing = 0;  // parameters the record does not carry
};

namespace detail {

// Writes `v` into, or reads a value from, the two (`put_u16`, `get_u16`) or
// four (`put_u32`, `get_u32`) bytes of the span starting at index `at`,
// least significant byte first, whatever the byte order of the host. The
// bounds are not checked: the behavior is undefined if those bytes do not
// all lie within the span.
constexpr void put_u16(std::span<std::byte> out,
                       std::size_t at,
                       std::uint16_t v)
{
  out[at] = static_cast<std::byte>(v & 0xFFu);
  out[at + 1] = static_cast<std::byte>((v >> 8) & 0xFFu);
}

constexpr void put_u32(std::span<std::byte> out,
                       std::size_t at,
                       std::uint32_t v)
{
  for (auto i = 0uz; i < 4; ++i)
    out[at + i] = static_cast<std::byte>((v >> (8 * i)) & 0xFFu);
}

constexpr std::uint16_t get_u16(std::span<std::byte const> in, std::size_t at)
{
  return static_cast<std::uint16_t>(
      std::to_integer<std::uint16_t>(in[at])
      | (std::to_integer<std::uint16_t>(in[at + 1]) << 8));
}

constexpr std::uint32_t get_u32(std::span<std::byte const> in, std::size_t at)
{
  std::uint32_t v = 0;
  for (auto i = 0uz; i < 4; ++i)
    v |= std::to_integer<std::uint32_t>(in[at + i]) << (8 * i);
  return v;
}

// Computes the CRC-32 of `data` in the variant of zlib and Ethernet
// (CRC-32/ISO-HDLC): polynomial `0x04C11DB7`, input and output reflected
// (the reflected polynomial is `0xEDB88320`), initial value `0xFFFFFFFF`,
// final XOR with `0xFFFFFFFF`. The check value, i.e. the CRC of the ASCII
// string `"123456789"`, is `0xCBF43926`.
constexpr std::uint32_t crc32(std::span<std::byte const> data)
{
  std::uint32_t crc = 0xFFFFFFFFu;
  for (auto byte : data) {
    crc ^= std::to_integer<std::uint32_t>(byte);
    for (auto i = 0; i < 8; ++i)
      crc = (crc >> 1) ^ ((crc & 1u) ? 0xEDB88320u : 0u);
  }
  return crc ^ 0xFFFFFFFFu;
}

} // namespace detail

// Encodes `values` as a record at the beginning of `dest`, with `magic` in
// both magic fields and `seq` as the sequence number, and returns the size
// of the record, i.e. `record_size(image<Schema>::count)`. If `dest` is
// shorter than that, returns zero and leaves `dest` unchanged. The bytes of
// `dest` past the record are not written.
//
// A store writes the body of the record to the medium before its footer;
// see `record_format`.
template<auto& Schema>
constexpr std::size_t encode_record(std::span<std::byte> dest,
                                    image<Schema> const& values,
                                    std::uint32_t magic,
                                    std::uint32_t seq)
{
  constexpr auto count = schema_t<Schema>::count;
  static_assert(count <= UINT16_MAX, "too many parameters for one record");

  constexpr auto size = record_size(count);
  if (dest.size() < size) return 0;

  detail::put_u32(dest, 0, magic);
  detail::put_u16(dest, 4, record_format);
  detail::put_u16(dest, 6, static_cast<std::uint16_t>(count));
  detail::put_u32(dest, 8, seq);
  detail::put_u32(dest, 12, schema_id<Schema>());

  for (auto i = 0uz; i < count; ++i) {
    auto const at = record_header_size + (i * record_cell_size);
    detail::put_u32(dest, at, Schema.parameters[i].id);
    detail::put_u32(dest, at + 4, values.cell(i));
  }

  detail::put_u32(dest, size - 8, magic);
  detail::put_u32(dest, size - 4, detail::crc32(dest.first(size - 4)));
  return size;
}

// Decodes the header of a record from the first `record_header_size` bytes
// of `src` and returns it, or returns `std::nullopt` if `src` is shorter
// than that, if the stored magic differs from `magic`, or if the stored
// format differs from `record_format`.
//
// Checks nothing else: neither `count` nor the footer nor the CRC. A header
// it returns may belong to a torn or corrupted record, which `check_record`
// and `decode_record` detect.
constexpr std::optional<record_header>
decode_header(std::span<std::byte const> src, std::uint32_t magic)
{
  if (src.size() < record_header_size) return std::nullopt;

  record_header const header{.magic = detail::get_u32(src, 0),
                             .format = detail::get_u16(src, 4),
                             .count = detail::get_u16(src, 6),
                             .seq = detail::get_u32(src, 8),
                             .schema_id = detail::get_u32(src, 12)};

  if (header.magic != magic) return std::nullopt;
  if (header.format != record_format) return std::nullopt;
  return header;
}

// Returns the header of the record at the beginning of `src` if the record
// is whole, or `std::nullopt` otherwise. Bytes of `src` past the record are
// ignored.
//
// The record is whole if its header stores `magic` and `record_format`,
// `src` holds the `count` cells the header declares and the footer, the
// footer repeats `magic`, and the stored CRC equals `detail::crc32` of all
// bytes before it. Computes the CRC, a pass over the record, only if every
// other condition holds.
constexpr std::optional<record_header>
check_record(std::span<std::byte const> src, std::uint32_t magic)
{
  auto const header = decode_header(src, magic);
  if (!header) return std::nullopt;

  auto const size = record_size(header->count);
  if (src.size() < size) return std::nullopt;
  if (detail::get_u32(src, size - 8) != magic) return std::nullopt;
  if (detail::get_u32(src, size - 4) != detail::crc32(src.first(size - 4))) {
    return std::nullopt;
  }
  return header;
}

// Decodes the record at the beginning of `src` into `values` and returns a
// `load_report` of the outcome. Bytes of `src` past the record are ignored.
//
// If the record is not whole, i.e. if `check_record` rejects it for `magic`,
// returns a report whose `valid` is `false` and leaves `values` unchanged.
// Otherwise, sets every parameter in `values` to its default value, then
// assigns each cell to the parameter of `Schema` with its identifier, if
// there is one and the value lies in that parameter's range as checked by
// `in_range`. A parameter whose cell is rejected or missing thus holds its
// default value.
template<auto& Schema>
constexpr load_report decode_record(std::span<std::byte const> src,
                                    std::uint32_t magic,
                                    image<Schema>& values)
{
  load_report report;

  auto const header = check_record(src, magic);
  if (!header) return report;

  report.valid = true;
  report.seq = header->seq;
  report.stored = header->count;
  report.schema_matched = (header->schema_id == schema_id<Schema>());

  values.restore_defaults();

  for (auto i = 0uz; i < header->count; ++i) {
    auto const at = record_header_size + (i * record_cell_size);
    auto const id = detail::get_u32(src, at);
    auto const cell = detail::get_u32(src, at + 4);

    auto const index = Schema.find(id);
    if (!index) {
      ++report.unknown;
      continue;
    }

    auto const& desc = Schema.parameters[*index];
    if (!in_range(desc.type, cell, desc.min, desc.max)) {
      ++report.rejected;
      continue;
    }

    values.assign_cell(*index, cell);
    ++report.loaded;
  }

  auto const covered = static_cast<std::size_t>(report.loaded)
                     + static_cast<std::size_t>(report.rejected);
  report.missing = static_cast<std::uint16_t>(
      covered < schema_t<Schema>::count ? schema_t<Schema>::count - covered
                                        : 0);
  return report;
}

} // namespace settings
} // namespace emb
