#pragma once

#include <emb/settings/image.hpp>
#include <emb/settings/schema.hpp>

#include <optional>
#include <span>

#include <cstddef>
#include <cstdint>

namespace emb {
namespace settings {

// The stored form of a whole section: a self-describing record, laid out
// so that it can be committed atomically on any medium.
//
//   0x00   4   magic       whose section this is
//   0x04   2   format      layout version of the record itself
//   0x06   2   count       cells that follow
//   0x08   4   seq         write counter, compared modulo 2^32
//   0x0C   4   schema_id   fingerprint of the identifiers that wrote it
//   0x10  8N   cells       { u32 id; u32 value }
//   ...    4   magic       repeated, so a torn record is visible at a glance
//   ...    4   crc32       over everything before it
//
// Cells carry identifiers, not positions, so adding, removing or reordering
// parameters is not a breaking change and the record needs no schema
// version: the directory is the migration mechanism. The identifier mixes
// the parameter's type in, so retyping under the same name simply stops
// matching and the parameter comes up with its default.
//
// Header and footer are sized so that both the body and the footer are
// multiples of eight bytes: every medium's write granularity up to eight
// divides them, and a store can therefore write the body first and the
// footer last on all of them. That order is what makes a record atomic —
// an interrupted write leaves no valid crc, and the previous record stands
// untouched.
//
// This header knows nothing about media or slots. It turns an image into
// bytes and bytes back into an image; where those bytes live is the store's
// business.

inline constexpr std::uint16_t record_format = 1;

inline constexpr std::size_t record_header_size = 16;
inline constexpr std::size_t record_cell_size = 8;
inline constexpr std::size_t record_footer_size = 8;

// Header and cells: what a store writes first.
constexpr auto record_body_size(std::size_t count) -> std::size_t
{
  return record_header_size + count * record_cell_size;
}

constexpr auto record_size(std::size_t count) -> std::size_t
{
  return record_body_size(count) + record_footer_size;
}

struct record_header {
  std::uint32_t magic;
  std::uint16_t format;
  std::uint16_t count;
  std::uint32_t seq;
  std::uint32_t schema_id;
};

// Which of two records is the later one. Modular, so the counter may wrap
// without a rollover ever being mistaken for an ancient record.
constexpr bool seq_newer(std::uint32_t a, std::uint32_t b)
{
  return static_cast<std::int32_t>(a - b) > 0;
}

// A fingerprint of the declared identifiers, in order. Not a compatibility
// gate — the directory handles that — but it tells an operator whether a
// record was written by this build of the schema or another one.
template<auto& Schema>
consteval auto schema_id() -> std::uint32_t
{
  std::uint32_t h = 0x811C9DC5u;
  for (auto const& p : Schema.parameters)
    for (auto shift = 0; shift < 32; shift += 8) {
      h ^= (p.id >> shift) & 0xFFu;
      h *= 0x01000193u;
    }
  return h;
}

// What a decode found. Every count is a fact an operator may need: "the
// parameter did not read" and "the parameter is like that" are different
// things, and today's stack cannot tell them apart.
struct load_report {
  bool valid = false;          // a well-formed record was parsed
  bool schema_matched = false; // written by this build of the schema
  std::uint32_t seq = 0;
  std::uint16_t stored = 0;   // cells the record carried
  std::uint16_t loaded = 0;   // cells accepted into the image
  std::uint16_t unknown = 0;  // identifiers this firmware no longer has
  std::uint16_t rejected = 0; // outside the range their descriptor allows
  std::uint16_t missing = 0;  // parameters the record did not carry
};

namespace detail {

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

constexpr auto get_u16(std::span<std::byte const> in, std::size_t at)
    -> std::uint16_t
{
  return static_cast<std::uint16_t>(
      std::to_integer<std::uint16_t>(in[at])
      | (std::to_integer<std::uint16_t>(in[at + 1]) << 8));
}

constexpr auto get_u32(std::span<std::byte const> in, std::size_t at)
    -> std::uint32_t
{
  std::uint32_t v = 0;
  for (auto i = 0uz; i < 4; ++i)
    v |= std::to_integer<std::uint32_t>(in[at + i]) << (8 * i);
  return v;
}

// The ordinary reflected CRC-32, computed a bit at a time: a table would
// cost a kilobyte of flash to save microseconds on an operation that
// happens twice a boot.
constexpr auto crc32(std::span<std::byte const> data) -> std::uint32_t
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

// Lays the whole record out, crc included, and returns its size; zero if
// the buffer is too small. A store writes the first record_body_size()
// bytes, then the rest — never the other way round.
template<auto& Schema>
constexpr auto encode_record(std::span<std::byte> dest,
                             image<Schema> const& values,
                             std::uint32_t magic,
                             std::uint32_t seq) -> std::size_t
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

// What a slot scan reads: enough to tell whether a slot holds a record of
// this section at all, and how recent it is. Says nothing about integrity —
// only a full decode does.
constexpr auto decode_header(std::span<std::byte const> src,
                             std::uint32_t magic)
    -> std::optional<record_header>
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

// Parses a record into the image. Nothing is written unless the record is
// whole: an invalid one leaves the image exactly as it was, so a store can
// try the other slot and only then fall back to defaults.
//
// A valid record starts from the defaults, so a parameter the record does
// not carry — one this firmware added — comes up defined rather than
// keeping whatever the image held.
template<auto& Schema>
constexpr auto decode_record(std::span<std::byte const> src,
                             std::uint32_t magic,
                             image<Schema>& values) -> load_report
{
  load_report report;

  auto const header = decode_header(src, magic);
  if (!header) return report;

  auto const size = record_size(header->count);
  if (src.size() < size) return report;
  if (detail::get_u32(src, size - 8) != magic) return report;
  if (detail::get_u32(src, size - 4) != detail::crc32(src.first(size - 4))) {
    return report;
  }

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
    if (index == schema_t<Schema>::npos) {
      ++report.unknown;
      continue;
    }

    auto const& desc = Schema.parameters[index];
    if (!in_range(desc.type, cell, desc.min, desc.max)) {
      ++report.rejected;
      continue;
    }

    values.assign_cell(index, cell);
    ++report.loaded;
  }

  report.missing = static_cast<std::uint16_t>(schema_t<Schema>::count
                                              - report.loaded);
  return report;
}

} // namespace settings
} // namespace emb
