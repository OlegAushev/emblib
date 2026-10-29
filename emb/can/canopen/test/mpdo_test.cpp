#include <emb/can/canopen/detail/od_check.hpp>
#include <emb/can/canopen/detail/tpdo_producer.hpp>
#include <emb/can/canopen/mpdo.hpp>
#include <emb/can/canopen/od.hpp>
#include <emb/can/canopen/od_description.hpp>
#include <emb/can/canopen/od_dictionary.hpp>
#include <emb/can/canopen/od_handlers.hpp>
#include <emb/can/canopen/od_process_data.hpp>
#include <emb/can/canopen/od_trouble.hpp>
#include <emb/can/canopen/pdo.hpp>
#include <emb/can/canopen/server.hpp>
#include <emb/can/canopen/types.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include <emb/can.hpp>
#include <emb/test/mock/can_bus.hpp>

namespace {

using namespace emb::can::canopen;
using namespace std::chrono_literals;
using emb::can::payload_t;

// -- Frames --

// A frame in source address mode: the producer in byte 0, with bit 7 clear,
// the index, little-endian, the subindex, and the data.
static_assert(to_payload(sam_mpdo{.producer = node_id{5},
                                  .index = 0x4000,
                                  .subindex = 0x07,
                                  .data = {0x04, 0x00, 0x00, 0x00}})
              == payload_t{0x05, 0x00, 0x40, 0x07, 0x04, 0x00, 0x00, 0x00});
static_assert(to_payload(sam_mpdo{.producer = node_id{127},
                                  .index = 0x1234,
                                  .subindex = 0xFE,
                                  .data = {0x01, 0x02, 0x03, 0x04}})
              == payload_t{0x7F, 0x34, 0x12, 0xFE, 0x01, 0x02, 0x03, 0x04});

// A value travels as the data of an expedited SDO carries it, at compile
// time too: the bytes of its scalar, little-endian, then zeros.
static_assert(to_sdo_data(od_value{true}) == expedited_sdo_data{1, 0, 0, 0});
static_assert(to_sdo_data(od_value{std::uint8_t{4}})
              == expedited_sdo_data{4, 0, 0, 0});
static_assert(to_sdo_data(od_value{std::int8_t{-1}})
              == expedited_sdo_data{0xFF, 0, 0, 0});
static_assert(to_sdo_data(od_value{std::int16_t{-2}})
              == expedited_sdo_data{0xFE, 0xFF, 0, 0});
static_assert(to_sdo_data(od_value{std::uint32_t{0x12345678}})
              == expedited_sdo_data{0x78, 0x56, 0x34, 0x12});
static_assert(to_sdo_data(od_value{1.0f})
              == expedited_sdo_data{0x00, 0x00, 0x80, 0x3F});

// A list of the objects of an MPDO is an array of `mpdo_scan`, by the kind of
// the array.
static_assert(some_mpdo_scans<mpdo_scan[2]>);
static_assert(some_mpdo_scans<std::array<mpdo_scan, 1>>);
static_assert(!some_mpdo_scans<mpdo_scan>);
static_assert(!some_mpdo_scans<od_key[1]>);

// -- A record of statuses that the context holds --

// The context of the tests: the severities of up to 39 statuses, which the
// readers of a record read, and whether the reader of each fails. The
// registry of emb::trouble is not read at compile time.
struct ctx {
  std::array<std::uint8_t, 39> values{};
  std::array<bool, 39> failing{};
};

// Reads the status at subindex `Sub`, or fails if the context says so.
template<std::size_t Sub>
constexpr od_read_result read_status(ctx& c, std::uint16_t)
{
  if (c.failing[Sub - 1]) {
    return std::unexpected(sdo_abort_code::no_data_available);
  }
  return od_value{c.values[Sub - 1]};
}

template<std::size_t N>
constexpr od_read_result read_highest(ctx&, std::uint16_t)
{
  return od_value{static_cast<std::uint8_t>(N)};
}

// The layout of a record of `N` statuses: 00h, then status k, `ro` `uint8`,
// at subindex k.
template<std::size_t N>
inline constexpr auto status_layout = [] {
  std::array<od_field<ctx>, N + 1> fields{};
  fields[0] = {.access = od_access::const_,
               .type = od_data_type::uint8,
               .read = &read_highest<N>};
  [&]<std::size_t... K>(std::index_sequence<K...>) {
    ((fields[K + 1] = {.access = od_access::ro,
                       .type = od_data_type::uint8,
                       .read = &read_status<K + 1>}),
     ...);
  }(std::make_index_sequence<N>{});
  return fields;
}();

template<std::size_t K>
inline constexpr auto status_name = "status_" + detail::od_decimal<K>;

template<std::size_t K>
inline constexpr auto status_title_en = "Status " + detail::od_decimal<K>;

template<std::size_t K>
inline constexpr auto status_title_ru = "Статус " + detail::od_decimal<K>;

template<std::size_t N>
inline constexpr auto status_labels = [] {
  std::array<od_field_label, N + 1> labels{};
  labels[0] = {.name = "highest_subindex",
               .title = {"Highest subindex", "Наибольший подындекс"}};
  [&]<std::size_t... K>(std::index_sequence<K...>) {
    ((labels[K + 1] = {.name = status_name<K + 1>.view(),
                       .title = {status_title_en<K + 1>.view(),
                                 status_title_ru<K + 1>.view()}}),
     ...);
  }(std::make_index_sequence<N>{});
  return labels;
}();

template<std::size_t N>
inline constexpr od_binding<ctx> status_record{.access = od_access::ro,
                                               .type = od_data_type::uint8,
                                               .layout = status_layout<N>,
                                               .field_labels =
                                                   status_labels<N>};

inline constexpr od_area areas[] = {
    {0x4000, 0x4FFF, od_class::status},
    {0x5000, 0x5FFF, od_class::watch},
};

inline constexpr od_group groups[] = {
    {"trouble", {"Trouble", "Неисправности"}},
};

// clang-format off
inline constexpr od_row<ctx> few_rows[] = {
{{0x4000, 0x00}, {"trouble.statuses", {"Statuses", "Статусы"}}, status_record<5>},
};
inline constexpr od_row<ctx> many_rows[] = {
{{0x4000, 0x00}, {"trouble.statuses", {"Statuses", "Статусы"}}, status_record<39>},
};
// clang-format on

inline constexpr auto few_dictionary =
    make_dictionary<few_rows, areas, groups>();
inline constexpr auto many_dictionary =
    make_dictionary<many_rows, areas, groups>();

inline constexpr mpdo_scan few_objects[] = {{0x4000, 0x01, 5}};
inline constexpr mpdo_scan many_objects[] = {{0x4000, 0x01, 39}};

// The source of the statuses of node 1 at 4000h:01 on, of the dictionary
// `Dictionary` and the list `Scans`, and the context that it reads.
template<auto const& Dictionary, auto const& Scans>
struct source {
  ctx c{};
  mpdo_scanner<ctx, Scans> scanner{node_id{1}, od_view<ctx>{Dictionary}, c};

  // Asks the source for a frame and tells it that send took the frame, as a
  // slot does while the bus acknowledges its frames; returns the subindex of
  // the frame, or 0 if the source gave none.
  constexpr unsigned send()
  {
    auto const frame = scanner.next();
    if (!frame) return 0;
    scanner.sent();
    return (*frame)[3];
  }

  // Sends the frames that `subindices` names and checks that they go so.
  template<std::size_t M>
  constexpr bool sends(unsigned const (&subindices)[M])
  {
    return std::ranges::all_of(subindices,
                               [&](unsigned sub) { return send() == sub; });
  }
};

using few_source = source<few_dictionary, few_objects>;
using many_source = source<many_dictionary, many_objects>;

// -- Rule of selection --

// The first frame comes from the round: the first object, node 1 its
// producer, as it reads.
consteval bool test_first_frame()
{
  few_source s;
  return s.scanner.next()
      == payload_t{0x01, 0x00, 0x40, 0x01, 0x00, 0x00, 0x00, 0x00};
}

static_assert(test_first_frame());

// While nothing changes, the frames go round the list, an object a frame.
consteval bool test_round()
{
  few_source s;
  return s.sends({1, 2, 3, 4, 5, 1, 2, 3, 4, 5, 1});
}

static_assert(test_round());

// What changed goes before what did not, with its value: the frame after the
// first, a frame of the round, carries the change. The first search starts
// with the first object: of 01h and 03h changed after the first frame, 01h
// goes first.
consteval bool test_change_first()
{
  few_source s;
  if (s.send() != 1) return false;
  s.c.values[0] = 2;
  s.c.values[2] = 3;
  auto const frame = s.scanner.next();
  if (frame != payload_t{0x01, 0x00, 0x40, 0x01, 0x02, 0x00, 0x00, 0x00}) {
    return false;
  }
  s.scanner.sent();
  return s.sends({3, 2});
}

static_assert(test_change_first());

// The cursor of changes goes on from the last object it sent: of 02h and 04h
// changed after it sent 03h, 04h goes first, then 02h.
consteval bool test_changes_go_on()
{
  few_source s;
  if (s.send() != 1) return false;
  s.c.values[2] = 1;
  if (s.send() != 3) return false;
  s.c.values[1] = 1;
  s.c.values[3] = 1;
  return s.sends({4, 2});
}

static_assert(test_changes_go_on());

// A status raised goes as a change, and so does its clearing: the cursor of
// changes searches its own object last, so the clearing goes in the first or
// the second frame after it, as a change, and not with the round.
consteval bool test_cleared()
{
  few_source s;
  if (s.send() != 1) return false;
  s.c.values[2] = 4;
  if (s.send() != 3) return false;
  s.c.values[2] = 0;
  if (s.send() != 3) return false;
  // Raised again and cleared before the frame of the round: the clearing
  // goes in the frame after it.
  s.c.values[2] = 4;
  if (s.send() != 3) return false;
  s.c.values[2] = 0;
  return s.sends({2, 3});
}

static_assert(test_cleared());

// Every fourth frame comes from the round, and a frame of the round without
// a change does not reset the count: with six changes before frame 2, frames
// 0 to 8 are of the round, the round, two changes, the round, three changes
// and the round, and the sixth change follows.
consteval bool test_every_fourth()
{
  many_source s;
  if (!s.sends({0x01, 0x02})) return false;
  for (auto sub = 0x10uz; sub <= 0x15; ++sub) {
    s.c.values[sub - 1] = 1;
  }
  return s.sends({0x10, 0x11, 0x03, 0x12, 0x13, 0x14, 0x04, 0x15, 0x05});
}

static_assert(test_every_fourth());

// A status that changes before every frame does not stop the round: within
// 39 · 4 = 156 frames, every one of the 39 statuses goes at least once.
consteval bool test_full_picture()
{
  many_source s;
  std::array<bool, 39> seen{};
  for (auto frame = 0; frame < 156; ++frame) {
    s.c.values[0x22] ^= 1;
    seen[s.send() - 1] = true;
  }
  return std::ranges::all_of(seen, [](bool b) { return b; });
}

static_assert(test_full_picture());

// A reader that fails: its object is not sent and its change is not seen, the
// round passes on to the next object that reads, without sending that one
// twice, and the others go as before.
consteval bool test_failing_reader()
{
  few_source s;
  s.c.failing[2] = true;
  s.c.values[2] = 4;
  if (!s.sends({1, 2, 4, 5, 1, 2, 4})) return false;
  s.c.values[3] = 1;
  return s.sends({4, 5, 1});
}

static_assert(test_failing_reader());

// With no object that reads, there is no frame, and the count of frames does
// not step: once the readers read again, the frame that was due, a change,
// goes.
consteval bool test_no_object_reads()
{
  few_source s;
  if (!s.sends({1, 2, 3})) return false;
  for (auto k = 0uz; k < 5; ++k) {
    s.c.failing[k] = true;
  }
  if (s.scanner.next() || s.send() != 0) return false;
  s.c.failing = {};
  s.c.values[1] = 1;
  return s.sends({2, 4});
}

static_assert(test_no_object_reads());

// Without `sent` nothing moves: `next` chooses again, from the same state and
// the values as they are. A `sent` without a frame does nothing: the count of
// frames steps once for the frame, so that of three changes after it all go
// before the round.
consteval bool test_unsent()
{
  few_source s;
  if (s.scanner.next() != s.scanner.next()) return false;
  s.scanner.sent();
  s.scanner.sent();
  s.c.values[2] = 1;
  s.c.values[3] = 1;
  s.c.values[4] = 1;
  return s.sends({3, 4, 5, 2});
}

static_assert(test_unsent());

// -- The slot of an MPDO --

// TPDO1 of node 1, set up at 0 ms with a period of 10 ms with the source of
// the five statuses, as the server sets the slot of an MPDO up.
struct mpdo_tpdo1 {
  emb::test::can_bus bus;
  detail::tpdo_producer<1> producer{bus, node_id{1}};
  ctx c{};
  mpdo_scanner<ctx, few_objects> scanner{node_id{1},
                                         od_view<ctx>{few_dictionary},
                                         c};

  constexpr mpdo_tpdo1()
  {
    producer.setup<1, pdo_id::predefined()>(detail::scanned_source(scanner),
                                            10ms,
                                            0ms);
  }

  constexpr void run(std::chrono::milliseconds first,
                     std::chrono::milliseconds last)
  {
    for (auto now = first; now <= last; ++now) {
      producer.tick(now, nmt_state::operational);
    }
  }

  // The subindex and the value of the last frame that send took.
  constexpr unsigned subindex() const
  {
    return bus.last().payload[3];
  }

  constexpr unsigned value() const
  {
    return bus.last().payload[4];
  }
};

// The slot sends a frame of the source a period, on the COB-ID of TPDO1, 8
// bytes, and stamps no counter in it.
consteval bool test_slot()
{
  mpdo_tpdo1 t;
  t.run(1ms, 30ms);
  return t.bus.taken == 3
      && t.bus.last().id == cob_id_of<cob_type::tpdo, 1>(node_id{1})
      && t.bus.last().len == 8
      && t.bus.last().payload
             == payload_t{0x01, 0x00, 0x40, 0x03, 0x00, 0x00, 0x00, 0x00};
}

static_assert(test_slot());

// While the transmit queue of the driver is full, the slot does not tell the
// source of the frames that send refused: its shadow, its cursors and its
// count of frames stand. Once send takes frames again, the source goes on from
// where it stood, and what changed meanwhile goes by the rule of selection:
// the change due at the fourth frame, then the round, which carries the other
// change as it is, and nothing changed is left.
consteval bool test_full_queue()
{
  mpdo_tpdo1 t;
  t.run(1ms, 30ms);
  if (t.bus.taken != 3 || t.subindex() != 3) return false;
  t.bus.set_queue_full(true);
  t.c.values[3] = 2;
  t.c.values[1] = 1;
  t.run(31ms, 60ms);
  if (t.bus.refused != 3 || t.bus.taken != 3) return false;
  t.bus.set_queue_full(false);
  t.run(61ms, 70ms);
  if (t.bus.taken != 4 || t.subindex() != 2 || t.value() != 1) return false;
  t.run(71ms, 80ms);
  if (t.subindex() != 4 || t.value() != 2) return false;
  t.run(81ms, 90ms);
  return t.subindex() == 5 && t.bus.taken == 6;
}

static_assert(test_full_queue());

// -- Description --

inline constexpr od_pdo statuses_tpdo{.name = "statuses",
                                      .title = {"Statuses", "Статусы"},
                                      .direction = pdo_direction::tx,
                                      .number = 1,
                                      .cob_id = pdo_id::predefined(),
                                      .period = 10ms};

// The constant of an MPDO, before that of a PDO of rows in this translation
// unit: clang 22 would refuse the rows of the second if the concept of a list
// went by the type of a member rather than by the kind of the array.
inline constexpr auto statuses_pdo = pdo_of<few_objects>(statuses_tpdo);

struct level {
  std::uint8_t value;
};

// clang-format off
inline constexpr pdo_row<level> level_rows[] = {
{{0, 8}, {"value", {"Level", "Уровень"}}, pdo_map<&level::value>},
};
// clang-format on

inline constexpr auto level_pdo =
    pdo_of<level_rows>({.name = "level",
                        .title = {"Level", "Уровень"},
                        .direction = pdo_direction::tx,
                        .number = 2,
                        .cob_id = pdo_id::predefined(),
                        .period = 100ms});

// The type of the MPDO knows its list, and the MPDO converts to what the
// description reads: the list, and no signal.
inline constexpr od_mapped_pdo mapped_statuses = statuses_pdo;
inline constexpr od_mapped_pdo mapped_level = level_pdo;

static_assert(&mapped_list<statuses_pdo> == &few_objects);
static_assert(&mapped_statuses.pdo == &statuses_pdo.pdo);
static_assert(mapped_statuses.mapping() == pdo_mapping::sam);
static_assert(mapped_statuses.scans.data() == few_objects);
static_assert(mapped_statuses.scans.size() == 1);
static_assert(mapped_statuses.signals.empty());
static_assert(mapped_level.mapping() == pdo_mapping::signals);
static_assert(mapped_level.scans.empty());

inline constexpr od_mapped_pdo described_pdos[] = {level_pdo, statuses_pdo};
inline constexpr od_process_data described{.pdos = described_pdos};

inline constexpr auto description =
    make_description<few_rows, areas, groups, "Test device", described>();

consteval std::string_view as_view(auto const& chars)
{
  return {chars.data(), chars.size()};
}

// The MPDO is a line pdo of the mapping sam, with no line signal after it,
// and the PDO of signals follows it, as TPDO2.
inline constexpr std::string_view described_pdo_lines =
    R"({"type":"pdo","name":"statuses","title":"Statuses","title_ru":"Статусы","direction":"tx","number":1,"mapping":"sam","cob_base":"0x180","period_ms":10})"
    "\n"
    R"({"type":"pdo","name":"level","title":"Level","title_ru":"Уровень","direction":"tx","number":2,"mapping":"signals","cob_base":"0x280","period_ms":100})"
    "\n"
    R"({"type":"signal","pdo":"level","start":0,"width":8,"role":"value","name":"value","title":"Level","title_ru":"Уровень","encoding":"unsigned"})"
    "\n";

static_assert(as_view(description).ends_with(described_pdo_lines));
static_assert(
    od_description_fingerprint(description)
    == make_dictionary<few_rows, areas, groups, described>().fingerprint());

// The server sets the slot of an MPDO up with a source of the list of the
// MPDO, and with no other.
template<auto const& Mapped, typename Scanner>
concept sets_up = requires(server<server_options{}, ctx>& s, Scanner& source) {
  s.template setup_tpdo<Mapped>(source);
};

inline constexpr mpdo_scan same_objects[] = {{0x4000, 0x01, 5}};

static_assert(sets_up<statuses_pdo, mpdo_scanner<ctx, few_objects>>);
static_assert(!sets_up<statuses_pdo, mpdo_scanner<ctx, same_objects>>);
static_assert(!sets_up<level_pdo, mpdo_scanner<ctx, few_objects>>);

// The source of the slot of an MPDO has no counter.
consteval bool test_no_counter()
{
  few_source s;
  return detail::scanned_source(s.scanner).counter_mask == 0;
}

static_assert(test_no_counter());

// -- Checks --

constexpr std::uint8_t read_load()
{
  return 0;
}

constexpr void write_clear(std::uint32_t) {}

// The record of five statuses at 4000h, a command at 4100h:01 of the class
// status, and an object at 5000h:01 of the class watch.
// clang-format off
inline constexpr od_row<ctx> checked_rows[] = {
{{0x4000, 0x00}, {"trouble.statuses", {"Statuses", "Статусы"}}, status_record<5>},
{{0x4100, 0x01}, {"trouble.clear", {"Clear", "Сбросить"}},      od_wo<write_clear>},
{{0x5000, 0x01}, {"trouble.load", {"Load", "Нагрузка"}},        od_ro<read_load>},
};
// clang-format on

// Checks the MPDO `pdo`, TPDO1 `statuses` by default, that sends the list
// `Scans`, beside the dictionary of the record and the two objects.
template<auto const& Scans>
consteval std::string check_list(od_pdo const& pdo = statuses_tpdo)
{
  auto const mapped = pdo_of<Scans>(pdo);
  od_mapped_pdo const pdos[] = {mapped};
  return detail::od_check(checked_rows, areas, groups, {.pdos = pdos});
}

// A block or one object, in any mix; 00h and the implied 00h of an index of
// plain objects read too.
inline constexpr mpdo_scan whole_list[] = {{0x4000, 0x00, 6}};
inline constexpr mpdo_scan mixed_list[] = {{0x4000, 0x02},
                                           {0x4000, 0x04, 2},
                                           {0x4000, 0x01}};
inline constexpr mpdo_scan implied_list[] = {{0x4100, 0x00}};

static_assert(check_list<few_objects>().empty());
static_assert(check_list<whole_list>().empty());
static_assert(check_list<mixed_list>().empty());
static_assert(check_list<implied_list>().empty());

// A block names at least one object and none past FEh.
inline constexpr mpdo_scan empty_block[] = {{0x4000, 0x01, 0}};
inline constexpr mpdo_scan past_fe[] = {{0x4000, 0xF0, 16}};
inline constexpr mpdo_scan up_to_fe[] = {{0x4000, 0xEF, 16}};

static_assert(check_list<empty_block>()
              == "od: TPDO1 statuses: the block at 4000h:01 has no object");
static_assert(check_list<past_fe>()
              == "od: TPDO1 statuses: the block of 16 objects at 4000h:F0 "
                 "runs past subindex FEh");
static_assert(check_list<up_to_fe>()
              == "od: TPDO1 statuses: 4000h:EF is no object of the "
                 "dictionary");

// Each object is one that the dictionary serves: not past the layout of a
// record, and not at an index that no row has.
inline constexpr mpdo_scan past_layout[] = {{0x4000, 0x04, 3}};
inline constexpr mpdo_scan no_index[] = {{0x4200, 0x01}};
inline constexpr mpdo_scan no_subindex[] = {{0x4100, 0x02}};

static_assert(check_list<past_layout>()
              == "od: TPDO1 statuses: 4000h:06 is no object of the "
                 "dictionary");
static_assert(check_list<no_index>()
              == "od: TPDO1 statuses: 4200h:01 is no object of the "
                 "dictionary");
static_assert(check_list<no_subindex>()
              == "od: TPDO1 statuses: 4100h:02 is no object of the "
                 "dictionary");

// Each object is a status, and one that reads.
inline constexpr mpdo_scan watched[] = {{0x5000, 0x01}};
inline constexpr mpdo_scan write_only[] = {{0x4100, 0x00, 2}};

static_assert(check_list<watched>()
              == "od: TPDO1 statuses: 5000h:01 is of the class watch, and an "
                 "MPDO sends statuses alone");
static_assert(check_list<write_only>()
              == "od: TPDO1 statuses: 4100h:01 is not readable");

// No object is sent twice: blocks of one index do not overlap.
inline constexpr mpdo_scan overlapping[] = {{0x4000, 0x01, 3},
                                            {0x4000, 0x03, 2}};
inline constexpr mpdo_scan repeated[] = {{0x4000, 0x02},
                                         {0x4000, 0x04},
                                         {0x4000, 0x02}};
inline constexpr mpdo_scan adjoining[] = {{0x4000, 0x01, 2}, {0x4000, 0x03, 3}};

static_assert(check_list<overlapping>()
              == "od: TPDO1 statuses: 4000h:03 is sent twice");
static_assert(check_list<repeated>()
              == "od: TPDO1 statuses: 4000h:02 is sent twice");
static_assert(check_list<adjoining>().empty());

// An MPDO in source address mode is a TPDO.
static_assert(check_list<few_objects>({.name = "statuses",
                                       .title = {"Statuses", "Статусы"},
                                       .direction = pdo_direction::rx,
                                       .number = 1,
                                       .cob_id = pdo_id::predefined(),
                                       .timeout = 100ms})
              == "od: RPDO1 statuses: an MPDO in source address mode is a "
                 "TPDO: the device sends it");

// A description has at most one MPDO in source address mode.
consteval std::string check_two_mpdos()
{
  auto const first = pdo_of<few_objects>(statuses_tpdo);
  auto const second = pdo_of<same_objects>({.name = "more",
                                            .title = {"More", "Ещё"},
                                            .direction = pdo_direction::tx,
                                            .number = 3,
                                            .cob_id = pdo_id::predefined(),
                                            .period = 10ms});
  od_mapped_pdo const pdos[] = {first, level_pdo, second};
  return detail::od_check(checked_rows, areas, groups, {.pdos = pdos});
}

static_assert(check_two_mpdos()
              == "od: TPDO1 statuses and TPDO3 more are both MPDOs in source "
                 "address mode, and a device sends one");

} // namespace
