# Settings and NVM: design and migration plan

Status: done. Every step of the plan is implemented: the settings live in
a schema with bounds and apply policies; they are stored as whole
records that commit or do not; they are reached by declaration from the
application and by index from a protocol; and a change takes effect without
a restart, on command, wherever the application admits it. The stack it
replaced is gone.

Two parts of the design never became steps and are not built: the debounced
save (§10a) and the sections beyond `config` (§5); the trigger that applies
a batch whole came with applying on command. Checked against the code on 2026-09-28. The
CANopen binding (§7) was built on 2026-09-29, as a binding per row of the
dictionary rather than a generated section. On 2026-10-02 the facade the
application kept in `params.hpp/.cpp` moved into emblib as
`settings::section`, and what had been `section`, the place on a medium,
became `placement`. The same day the store stopped letting a run of failed
saves reach the newest record, and a load that restores nothing stopped
resetting the sequence (§10). On 2026-10-03 the store was rewritten to
keep nothing about the medium between calls: each call reads every slot
whole and trusts only whole records (§5, §10). The store's algorithms,
invariants and failure scenarios are described in detail, in Russian, in
`settings-store-algorithms.ru.md`. On 2026-10-05 the section stopped
marking the groups a write changed: a value waits to be applied while it
differs from the value applied, which the section keeps as a third image,
so a value written and put back owes neither a rebuild nor a restart (§4,
§10). On 2026-10-15 the application stopped naming a parameter by a
string: the typed access and a dictionary row take the parameter's
declaration, a `constexpr` variable the schema lists (§3, §7, §10). On
2026-10-16 the application stopped applying settings by itself: a write
requests a value, and a command applies it, all or nothing, through an
applier the application hands the section, which checks, admits and
configures each group of the change. The configs are built from the values
applied only, a save keeps only what is applied or checked, and the
dictionary shows every parameter and every group as a record; 1011h:04 is
gone (§4, §7, §10). On 2026-10-17 the groups went: the applier compares
what it builds from the values applied before and after a change, a check
is a set of rules over the parameters each names, a rule that the values
loaded break falls back to the defaults of its inputs, and the dictionary
has no records of groups (§3, §4, §7, §10).

## 1. Scope

A replacement for the current NVM parameter stack (`emb/nvm.hpp` +
`app/inverter/settings/`) that:

- works the same on external FRAM, external EEPROM and internal flash;
- survives power loss during a write, and survives firmware upgrades that
  add, remove or retype parameters;
- lets an updated parameter take effect without a restart wherever that is
  physically possible, and says so honestly where it is not;
- binds to a CANopen object dictionary without the settings layer knowing
  that CANopen exists.

## 2. Layering

```
    emb::nvm          block storage concept, record format, slot store
        ^
        |
    emb::settings     schema, descriptors, RAM image, typed access, rules,
                      section
        ^                          ^
        |                          |
    app schema                 emb::can::canopen::od_settings
    (parameter list)           (adapter: OD table -> schema by declaration)
```

The dependency arrow never points from `settings` to a protocol. The
settings layer exposes a type-erased, protocol-neutral interface in its own
vocabulary; every transport (CANopen today, a console or Modbus map
tomorrow) builds its own table from the schema and maps errors at its own
boundary.

## 3. Parameter schema

One declaration per parameter, holding only facts about the parameter
itself:

```cpp
struct descriptor {
  std::string_view name;
  std::uint32_t    id;        // hash of name mixed with `kind`
  scalar_kind      kind;
  cell             def, min, max;  // ordered under `kind`
  apply_policy     apply;     // on_command / on_restart
  bool             writable;  // may ever change at run time
  bool             expose;    // meant for external access at all
};
```

Declared as:

```cpp
inline constexpr auto phase_swap =
  param("drive.phase_swap", false, {.apply = apply_policy::on_command});
inline constexpr auto runout_speed =
  param("drive.runout_speed", rpm{100.0f},
        {.min = rpm{0.0f}, .max = rpm{5000.0f},
         .apply = apply_policy::on_command});
inline constexpr auto stopping_torque =
  param("drive.stopping_torque", pu{0.05f},          // clamped: own bounds
        {.apply = apply_policy::on_command});

inline constexpr auto schema =
  settings::make_schema(phase_swap, runout_speed, stopping_torque);
```

The declaration is also the key of the typed access: `get<runout_speed>()`
returns an `rpm`, and `parameter<schema, runout_speed>` gives the index,
the default and the descriptor. The schema must hold the declaration
itself, one of the same name, type and descriptor, or the access does not
compile. The name remains what a stored value is matched by (§5): the
variable can be renamed and the value stays, the name cannot. A parameter
reached by index only may be declared in the `make_schema` call.

The type comes from the default, so a literal needs its suffix — `param("x",
0.05)` does not compile. Per-parameter checks (default within bounds, min
below max) live in `param()` so that a bad declaration is reported on its
own line; cross-parameter ones (duplicate name, identifier collision) live
in `make_schema`.

No offset and no size: every value occupies one four-byte cell, so a
parameter's index *is* its position in the RAM image, and the image is a
plain `std::array<cell, N>`. A bool costs three bytes of padding and
buys an image with no layout arithmetic and a record whose cells need no
per-entry size.

`writable` is not `od_access`: "immutable after production" (factory
calibration) is a fact about the parameter, while "read-only over SDO" is a
fact about the protocol and may be strictly narrower. A protocol may narrow,
never widen: the record of a parameter that is not `writable` shows its
value read-only (§7).

Type-erased interface offered to transports:

```cpp
namespace settings {
using value = std::variant<bool, std::int32_t, std::uint32_t, float>;
enum class error { unknown_parameter, read_only, type_mismatch, out_of_range };

std::span<descriptor const> descriptors();
std::optional<index> find(std::string_view name);
std::expected<value, error> get_at(index);
std::expected<void, error> set_at(index, value);
}
```

## 4. Applying changes without a restart

Parameters split into two classes, declared in the schema:

1. `on_command` — take effect when the application applies them on
   command: PI gains, limits, slopes, angle correction, protection
   thresholds, the machine's electrical parameters, the hall sensor's
   sector angles, the PWM frequency.
2. `on_restart` — change the *set* of objects that exist or the peripheral
   setup (`hall.enabled`), or are handed to an object only when it is built
   (`motor.p`, `isense.zero_drift_th`). Never applied on command; the
   section reports `restart_required` instead of pretending.

When a change may take effect is not the schema's to say. A "safe state"
means different things to different parameters — the phase order needs the
bridge off, the machine model of the observer a rotor that is not driven,
the gains nothing at all — and the application has to know it anyway. It
says so when a change is applied: its applier compares what it builds from
the values applied before the change and after it, and asks for a safe
state only where something that needs one differs. In the inverter
`change_between` tells which configs differ, and whether the change
touches the phase order, the machine, whether the current controllers are
tuned, or the calibration of the hall sensor: `drive.phase_swap` and the
protection thresholds feed one `drive_config`, yet only a change of the
phase order waits for the drive to stand.

Mechanism: **a command applies, through the application's applier, all or
nothing.**

- An SDO write validates and updates the values requested, nothing else.
- The section keeps, next to the values requested, the values applied, out
  of which the application builds its state, and the values stored. The
  config readers read the values applied only, so a value that waits never
  reaches a rebuild, whatever its policy.
- `apply(index, applier)` and `apply_all(applier)` choose the values of
  parameters applied on command that differ from the values applied — one
  or all — and apply them as one transaction, below. A check or an
  admission that refuses applies nothing and is kept as `last_refusal()`,
  as is every refusal of a save.
  `apply` refuses an `on_restart` parameter whatever its value; a choice
  with nothing in it succeeds without calling the applier.
- `discard` and `discard_all` put the values applied back into the values
  requested, of either policy, but not into a parameter that is not
  `writable`.

```
candidate = the values applied, with the chosen cells taken in
applier.check(candidate)                                 -- pure
guard = applier.lock()
applier.admit(the values applied, candidate)             -- state
before = the values applied; the values applied = candidate
applier.configure(before, the values applied)            -- cannot fail
the guard ends
```

The applier is the application's (`some_applier`): `check` is a pure
function of the values, in the inverter every rule holding (below), which
also runs before a save; `admit` compares the values applied before and
after the change and reads the state under the lock, so the state it read
holds until the configure ends; `configure` rebuilds what derives from the
parameters that differ, in an order the application keeps, hands each
config to its owner and cannot fail, which is why whatever can refuse is
asked before. Each is asked once per change. In the inverter
`settings::applier` masks the timebase and below; admits a change to the
phase order, the machine, autotune or the calibration of the hall sensor
only while the drive stands with its bridge off, in a safe state with the
model idle and the PWM stopped, since the bridge stops a step after
stopping hands over to ready; and has the drive rebuild the hall sensor's
sector maps with the PWM update and the hall capture masked too, since a
rotor that stands may still turn.

The inverter's configs compare with `==`, i.e. as numbers: a cell that
changes from `+0.0` to `-0.0` is recorded as applied and rebuilds nothing,
as no number changes. Whether a change waits for the drive to stand, each
config says in its `needs_standstill` by listing what may change while the
drive runs: anything else waits, so a field added later waits until it is
listed, and a config without a `needs_standstill` does not compile into
`change_between`.

Command, not pull. The seam used to be the drive's periodic task, which
took what it could every 33 ms: a value written while the drive ran took
effect, silently, whenever the drive next stopped, the client never
learned which, and a value that had to wait held its whole group back.
Applied on command, the answer to the command is the result. The work runs
in the SDO handler, in the main loop, the context the task ran in.

`configure()` decides explicitly what survives a reconfiguration: changing
`Kp/Ki` keeps the integrator (they are tuned while running, and a reset
would jerk the torque), changing motor parameters resets the MRAS observer.
Blind reconstruction has no vocabulary for that distinction, which is why
`configure()` beats "build a new object".

Anti-pattern, deliberately rejected: handing deep objects a reference into
the settings image to read parameters lazily. It makes `pmsm/` and `sense/`
depend on `settings/`, lets a value change between two reads inside one
control step, and throws away precomputation.

A save keeps what the machine runs on: it refuses while a value applied on
command waits, and checks the image, the values that wait for a restart
with it, before it writes. `restart_required()` counts an `on_restart`
value that differs from the one applied, requested or stored: a value
saved and then put back still owes the restart that would load it.

A check is a set of rules (`settings/rule.hpp`). A rule names the
parameters it relates, its inputs, as `inputs<schema, Decls...>`, a view
of an image that shows them and no other, and `holds(in)` reads them alone:
reading another parameter does not compile. A rule that holds on the
defaults of its inputs therefore holds whenever they are at their defaults,
whatever the other parameters hold, and `fall_back_to_defaults(rules)`
mends values loaded that break it: right after the load the application
has every rule they break fall back, the defaults of its inputs recorded
as applied, while the values loaded wait to be corrected or discarded.
Rules may share inputs, and a fallback that moves a shared input may break
another rule, so the passes repeat until every rule holds; each pass checks
every rule before it falls back any, so the order of the rules does not
matter, and a rule falls back at most once, its inputs staying at their
defaults. The program is ill-formed if a rule breaks on the defaults. In
the inverter the rules are `dc_window`, `ot_margins` and `hall_angles`: a
broken window of the DC voltage resets `prot.uvp_dc` and `prot.ovp_dc` and
nothing else. Until the fallback, values loaded that break a rule hold
back every change and every save, since a change is checked as the values
applied with it taken in.

A batch is applied whole by `apply_all`. One parameter at a time, each step
is checked as the values applied with that one value taken in, so values a
rule relates may have to be applied in an order: raising both bounds of a
window, the upper first. A calibration of the hall sensor goes from the
timebase interrupt into a mailbox of the drive; the main loop requests its
twelve angles, and an operator applies them with 2100h:01, the twelve
together: one at a time, a step either breaks the rule of the angles or
leaves a geometry that belongs to neither the old calibration nor the new.

## 5. NVM record format

One self-describing record per section, not a fixed-address cell array:

```
record (24 + 8N bytes):
  offset  size  field
  0x00     4    magic      section marker
  0x04     2    format     layout version of the record itself (not the schema)
  0x06     2    count      number of cells
  0x08     4    seq        sequence number, grows along the ring
  0x0C     4    schema_id  fingerprint of the identifiers that wrote it
  0x10  8*N     cells      N x { u32 id; u32 raw }
  ...      4    magic      repeated: a torn record is visible at a glance
  ...      4    crc32      over everything above; the footer commits
```

Header and footer are sized so that *both* the body and the footer are
multiples of eight bytes. Every plausible write granularity — 1 on FRAM, 4
on F4 word programming, 8 on a double-word medium — divides them, so the
same two writes commit a record everywhere. The repeated magic fills the
footer's second word with something useful instead of padding.

`schema_id` gates nothing: the directory already handles migration. It
tells an operator whether a record was written by this build of the schema
or another one.

- `raw` is the value bit-cast and zero-padded to 4 bytes. Every current type
  fits, which is also the expedited-SDO limit.
- Cell order is irrelevant: loading is a lookup by `id`. Adding, removing or
  reordering parameters is not a breaking change, and no schema version is
  needed in the record — the directory *is* the migration mechanism.
- `id = fnv1a32_continue(fnv1a32(name), kind)`, `kind` being the
  parameter's `scalar_kind`. A new scalar under the same name would
  otherwise pass unnoticed — same four bytes, same id, garbage in the control
  loop. Mixing the kind in makes the old cell simply not match, and the
  parameter comes up with its default. A new type of the same kind, e.g. a
  new unit, keeps the id, and the old number is read in the new unit: such a
  change renames the parameter. Uniqueness of `id` within a section is a
  compile-time assert.
- `seq` is compared modulo 2^32 (`int32_t(a - b) > 0`), so wrap is safe.

### Slots

Placement is declared once, as a value the store is instantiated with:

```cpp
inline constexpr placement config_placement{.magic = ..., .base = 0,
                                            .slot_capacity = 1024,
                                            .slot_count = 2};
// flash: .slot_count = 256, .slots_per_block = 128  // two 128 KiB sectors
```

Slots hold successive records; a save writes the next one and leaves the
previous intact. On every medium, slots are grouped into blocks the size
of the medium's erase unit, and a save that enters a block erases it
first; on FRAM, which has no erase unit, a block is one slot and erasing
it fills it with `erased_value`. The store keeps no position: a save reads
every slot, finds the newest whole record (see Load) and takes the first
slot after it, in its block, that reads erased; if none is left, it
erases the next block and takes its first slot. An empty section starts
at slot 0, erasing block 0. A record therefore always goes into erased
bytes, FRAM included, and the newest record's slot is never written, nor
its block erased. That is asserted, not assumed: the slots must span at
least two blocks, so the next block is never the newest record's (§10).

Each section declares `slot_capacity` as a **constant, not derived from the
current parameter count** — otherwise adding a parameter would shift the
slot stride and invalidate everything already stored.
`static_assert(record_size <= slot_capacity)`.

For the 60 parameters of the current product: record = 504 B,
`slot_capacity` = 1024 B (room for 125).

- FRAM (FM25W256, 32 KB): two slots, 2 KB of 32; endurance is a non-issue.
- EEPROM: identical; page splitting is the driver's business.
- Internal flash (APM32F405/F407): the top two sectors of the bank, 10 and
  11, 128 KiB each, which both linker scripts hold back from the code.
  Records are appended at `slot_capacity` stride — 128 slots per sector;
  on overflow switch to the other sector, erasing it on entry, and leave
  the old one holding the newest record until the next rollover erases
  it. One erase of each sector per 256 saves; at 10k cycles, on the order
  of a million saves. Atomicity and wear levelling from one mechanism.
  The price is a stalled CPU: programming halts instruction fetch, and
  erasing a sector halts it for the better part of a second.

The build picks the medium: `SETTINGS_STORAGE` selects the internal flash,
the default since it costs no board area, or the FRAM, where a save must
not stall the CPU. Both layouts live in the application's `hw/hw_nvm.hpp`,
since how big a slot is worth making and how many share an erase block are
facts about the memory.

### Commit protocol (identical on all three media)

1. Read every slot whole and decide from what is there (see Slots and
   Load): the slot, whether to erase the block it starts, and the sequence
   number — the newest record's plus the distance from its slot to the new
   one. A slot the medium refuses to read may hold the newest record, so a
   save that meets one writes nothing and fails with `save_stage::scan`;
   so does a save whose record would not come out newest, which is checked
   on what the pass found before anything is written (see §10).
2. Build the record in a RAM buffer — one burst instead of 60 small writes,
   and mandatory anyway for flash granularity.
3. Erase the block if the save enters one: a sector on flash, the slot
   itself on FRAM, filled with `erased_value`. The record goes into erased
   bytes on every medium.
4. Write header and cells.
5. **Write the footer last.** A record interrupted by power loss is not
   whole and is invisible to the loader, unless the tear left unwritten
   only bytes that already held their values — the last byte of the CRC
   is `0xFF` in about one record in 256, and the slot is erased — and
   then the next load restores it: a failed save leaves the old record or
   the new one. The previous record is untouched either way.
6. Read back and compare: the record must be whole and carry its sequence
   number and the CRC that was written. This catches a dead FRAM or a
   failed program — the stack it replaced had no such check — and, since
   the CRC is compared with the one written, a slot that holds another
   record under the same number. Nothing is kept in RAM, whatever the
   outcome: the next save reads the medium again and finds there what this
   one left — a record that landed although the save failed, or debris,
   which it steps over or erases.

### Load

1. Read every slot whole, once per load, as a save and a wipe do, and sort
   each: erased, a whole record (`check_record`: magic, format, a size that
   fits the slot, the footer's magic and the CRC), or anything else. A
   header with no whole record behind it counts for nothing; a slot the
   medium refuses to read is left out and reported.
2. The newest record is the whole record newer than every other under
   `seq_newer`; of two with one number, the one in the lower slot. Where
   none is newer than all the others — only on a medium the store before
   2026-10-03 numbered above a header whose top bit had rotted — the
   highest number taken unsigned wins.
3. Read the newest record again, decode it and require the number the
   pass saw; a read the medium refuses is retried once. A record that
   fails drops out and the next newest is tried. None left → all
   defaults + a reported fault.
4. Per cell: binary search `id` in the constexpr descriptor table sorted by
   id. Unknown id → ignored (removed parameter). Found → **validated against
   its bounds before entering the image**; out of range → default,
   counted. Corrupt storage must not inject a bad value into the loops.
5. Parameters absent from the record keep their defaults (new in this
   firmware).
6. Produce a load report — which slot and generation, whether the record
   was whole and written by this schema, whether a read failed, and how
   many values were stored, loaded, unknown, rejected and missing — and
   expose it for diagnostics (object `3000h` in the inverter). Before,
   "the parameter did not read" was indistinguishable from "the parameter
   is like that".

### Region layout

Sections are independent slot pairs at constexpr bases, asserted against the
medium capacity:

```
0x0000  config       slot A   1024
0x0400  config       slot B   1024
0x0800  calibration  slot A    256
0x0900  calibration  slot B    256
0x0A00  factory      slot A    128
```

A `factory` section on FRAM can additionally be hardware-protected: the
FM25W256 status register has `bp0/bp1/wpen`, so placing it in the top block
locks it with one SR write.

Only `config` is built, at the start of whichever medium the build picks;
`calibration` and `factory` are still a plan, and the hall calibration
lives in `config` meanwhile.

### Rejected alternatives

- **Fixed-offset cells (the scheme it replaced).** Same overhead (8 B per
  parameter), but no group atomicity, no migration, no flash compatibility.
- **Append log of single-parameter entries.** Minimal write amplification,
  but with deferred explicit `store()` writes are rare, so that buys
  nothing; it costs a full-sector scan and replay at boot, loses the atomic
  batch, and still needs compaction — which is exactly a whole-image write.
  Its real niche is different: **counters are not settings.** Hour meters,
  accumulated energy and fault counters need their own region, their own
  append log and their own wear strategy, and must never share a record with
  parameters.

## 6. Storage backend concept

`emb/nvm/storage.hpp`:

```cpp
template<typename T>
concept some_storage = requires {
  typename T::addr_type;
  typename T::error_type;
  { T::capacity }         -> std::convertible_to<std::size_t>;
  { T::write_granularity }-> std::convertible_to<std::size_t>;
  { T::needs_erase }      -> std::convertible_to<bool>;
  { T::erased_value }     -> std::convertible_to<std::byte>;
} && requires(T& s, typename T::addr_type addr,
              std::span<std::byte> out, std::span<std::byte const> in,
              std::size_t len) {
  { s.read(addr, out) }  -> std::same_as<std::expected<void, typename T::error_type>>;
  { s.write(addr, in) }  -> std::same_as<std::expected<void, typename T::error_type>>;
  { s.erase(addr, len) } -> std::same_as<std::expected<void, typename T::error_type>>;
};
```

- `erase` is required of every medium, including those with no erased state:
  it means "bring this range to `erased_value`", which keeps an explicit
  wipe honest on FRAM and lets every save write into erased bytes there
  too (§5). `needs_erase` separately says whether `write` *requires* an
  erased target; the store no longer asks.
- The error type belongs to the backend. Drivers already have a vocabulary
  (the FRAM driver's own reasons, with the bus's error as a cause; the
  flash driver's status flags); a common enum here would only add a
  mapping layer at the wrong end.
- Erase geometry is deliberately absent: F4 sectors are non-uniform
  (16K/64K/128K in one bank), so no constant describes them. Alignment is a
  property of the declared layout; the backend rejects a range that does not
  cover whole blocks.

## 7. CANopen binding

The OD table keeps what genuinely belongs to CANopen — index, display name,
categories, unit — and takes everything else from the section and its
schema through the parameter's declaration. The type is taken from the
schema as well, and written in the row besides, so that a text parser can
build a host's table from the source; the dictionary checks that the two
agree. Each parameter is a record, one row at an index of its own
(`can/canopen/od_settings.hpp`; the dictionary around it is described in
`canopen-od-algorithm.ru.md`):

```cpp
using setting = emb::can::canopen::od_settings<settings_of, applier_of>;
namespace params = settings::params;

inline constexpr emb::can::canopen::od_row<context> rows[] = {
  {{0x3100, 0x00}, "config", "drive",  "phase_swap", "",   boolean, setting::param<params::drive::phase_swap>},
  {{0x3107, 0x00}, "config", "drive",  "pwm_freq",   "Hz", float32, setting::param<params::drive::pwm_freq, pwm_frequency_actual>},
  // ...
};
```

`settings_of` and `applier_of` take the dictionary's context and return the
section and an applier; reached through the context, a section can live in
a test where constant evaluation may write it. The members of the record of
a parameter:

| subindex | access | member |
|---|---|---|
| 00h | const u8 | the last subindex, 09h, or 0Ah with an actual value |
| 01h | rw, ro if not `writable` | the value requested |
| 02h | ro | the value applied |
| 03h | ro | the value stored |
| 04h–06h | const | the default, the lower and the upper bound |
| 07h | const u8 | the policy: 0 on command, 1 on restart |
| 08h | wo u32 | applies the parameter |
| 09h | wo u32 | discards its value requested |
| 0Ah | ro | the actual value, with `param<Decl, Actual>` only |

`apply_all`, `discard_all` and `last_refusal` serve `od_exec` and `od_ro`;
`last_refusal`, 2100h:03, shows the abort of the last refusal of an
application or a save. `od_section_status` reads the state of the section,
3000h, through the same function of the context as the bridge.
A refusal answers with `to_sdo_abort`: what nothing applies on
command with `unsupported_access`, a failed check with
`parameter_incompatibility`, a change not admitted or values that wait to
be saved with `state_error`.

The `od_key <-> name` mapping stays hand-maintained on purpose: OD indices
are an external contract frozen for tools and EDS. Moving them into the
schema would not reduce maintenance, only put a protocol fact in the wrong
file.

Completeness is checked at compile time, without coupling:

- no duplicate `od_key` and no duplicate name in the dictionary, for every
  row, settings or not, and a record has its index to itself;
- every schema parameter with `expose` has exactly one record and a hidden
  one none — "forgot to publish the new parameter" fails the build;
- the type written in the row must equal the parameter's, `named_unit` and
  `emb::clamped` included (T15 in `canopen-od-algorithm.ru.md`), and an
  actual value must be read in that type.

1011h:04, which restored one default, is gone: a client reads 04h and
writes it to 01h. Restoring all defaults (2102h:01) requests them; they
take effect when applied.

## 8. File layout

```
external/emblib/emb/
  meta/fixed_string.hpp        [done] structural string for NTTP names
  nvm/storage.hpp              [done] block storage concept + is_erased
  settings/value.hpp           [done] scalar_kind, value, cell, conversions
  settings/param.hpp           [done] param(): def/min/max, writable,
                                      apply_policy, expose
  settings/schema.hpp          [done] make_schema, `parameter` by
                                      declaration, lookup by name and by id,
                                      descriptor table, uniqueness checks
  settings/image.hpp           [done] RAM image, typed and erased access
  settings/record.hpp          [done] record layout, encode and decode
  settings/store.hpp           [done] slots, active record, commit, load
                                      report
  settings/rule.hpp            [done] inputs, some_rule, all_hold: checks
                                      of parameters together, see §4
  settings/section.hpp         [done] one section: the values requested,
                                      stored and applied; load/save/wipe,
                                      apply on command, discard, fall back
                                      by rules
  can/canopen/od_settings.hpp  [done] records of the parameters of a
                                      section, see §7
  can/canopen/od_section.hpp   [done] the readers of a section's state
                                      under 3000h
  test/mock/ram_storage.hpp  [done] constexpr RAM backend for tests
  */test/*_test.cpp                   in-tree convention: next to the module,
                                      anonymous namespace, static_assert only

external/embdev/emb/dev/
  fm25w256.hpp                 [done] the driver itself models the concept:
                                      traits, erase(), default timeout; in
                                      src/common/nvm/ until 2026-09-09

external/mcudrv-apm32/apm32/f4/flash/
  flash.hpp                    [done] flash::region models it too: a run of
                                      whole sectors

src/app/inverter/hw/
  hw_nvm.hpp                   [done] both media and the section's layout
                                      on each; SETTINGS_STORAGE picks one

src/app/inverter/settings/
  schema.hpp                   [done] the product's parameter list, with
                                      bounds and apply policies
  params.hpp / params.cpp      [done] the config section; load/save/wipe
                                      and the service warnings as trouble
  settings.hpp / settings.cpp  [done] the config readers, what a change
                                      rebuilds, the rules, and the request
                                      of a hall calibration
  apply.hpp / apply.cpp        [done] the applier: when a change may take
                                      effect, and what it rebuilds
```

## 9. Implementation plan

Each step is a separate commit; every step through phase 1 leaves the
firmware behaviourally unchanged.

**Phase 0 — foundation in emblib, nothing existing is touched.**

1. `meta/fixed_string.hpp` — **done**
2. `nvm/storage.hpp` + mock backend — **done**
3. `settings/value.hpp` — **done**
4. `settings/param.hpp` + `settings/schema.hpp` — **done**
5. `settings/image.hpp` + `settings/pending.hpp` — **done**; the second
   has since gone: on 2026-10-05 the section replaced its masks with the
   values applied (§4, §10)
6. `settings/record.hpp` — **done**
7. `settings/store.hpp` — **done**
8. Store tests — **done**: two mock media (FRAM-like; flash-like with
   granularity, write-once and block erase) and the scenarios — clean
   memory, power cut while writing the body and at the commit, a corrupted
   newest record, a medium that acknowledges writes and keeps nothing,
   block rollover, an erase that must not take the last good record, wipe,
   saving before loading, and a record written by a richer firmware.

**Phase 1 — the application, alongside the old stack, nothing switched.**

9. `src/common/nvm/fm25w256_fram.hpp` — **done**: the driver models
   `some_storage` directly, no adapter; it has since moved to embdev
   as `emb/dev/fm25w256.hpp`
10. `src/app/inverter/settings/schema.hpp` — **done**: same parameters,
    plus bounds, groups and apply policies; checked against the old layout
    mechanically, same names in the same order with the same defaults
11. `src/app/inverter/settings/params.hpp/.cpp` — **done**

Not called by anything yet; instantiation was verified with `nm` on the
object file rather than the map, since `--gc-sections` drops unreferenced
code. The store, the image and the record codec are all instantiated
against the real FRAM driver there, so phase 0 is compiled for the target
and not only for the host.

Two names had to differ from the plan while both stacks coexist: the
facade's entry point is `load(memory)`, which pairs with `save()`, and the
whole-image reset is `restore_all_defaults()`, which pairs with
`restore_default_at(index)`. Both read better than the names they avoid.
The first has since lost its argument: the build picks the medium, and
`load()` binds the section to it.

**Phase 2 — consumers, one at a time.**

12-14. **done, and necessarily as one change**: reading from the new image
    while writes still went to the old registry would have left the firmware
    unable to persist anything, and `settings::init` — which built the old
    registry — had to go at the same time. So the config readers, the OD
    accessors and the save commands switched together. The OD accessors are
    per-row thunks into one shared by-index body, which needs no change to
    emblib's `od.hpp`. Binding the rows to the schema (dropping the
    hand-written type and default columns, and the thunks with them)
    followed on 2026-09-29, see §7.
15. `configure()` entry points and unapplied groups — **done**. The seam is
    the drive's periodic task tick, which already runs in task context with
    the control timebase masked: `motor_drive::apply_pending_settings()`
    decides from its own state what it can promise (`on_safe_state` when
    standing still, `live` otherwise) and applies the groups it can reach —
    drive, motor, model and hall, and pwm since the PWM frequency is
    stored. `pmsm::model`, `pmsm::mras_observer` and `hall::angle_sensor`
    grew a `configure()`; each keeps what it has learned, since gains are
    tuned while running. Latency is one task tick, 33 ms. Superseded on
    2026-10-16 by applying on command (§4): the task is gone, and
    `settings::applier` admits and configures each group when a command
    applies it.
16. Cleanup — **done**: `parameters.hpp`, `emb/nvm.hpp`, `od_nvm.hpp` and
    the registry's test are deleted rather than parked; git keeps the
    history. The one thing the FRAM driver still needed from the old header
    — an error vocabulary — became its own: the codes are the part's, not
    non-volatile memory's in general, which is what the storage concept
    says in the first place. Integrity left the vocabulary altogether; it
    is a record's business now, reported as a load result.

Verification after each step of phases 1-2: both presets
(`miniboard-debug`, `rev-a-debug`); for emblib headers additionally a host
`g++ -std=c++26 -Wall -Wextra -Wconversion` run.

Stored data is not migrated: the format is new and the first boot after
phase 2 comes up with defaults. Decided deliberately — no converter.

Since then the application has chosen its medium at build time (the
internal flash by default, §5), published the storage's layout, its
sequence number and the last load report under `3000h`, restricted a
restore of defaults to the parameters a protocol may write, and stored the
PWM frequency as `drive.pwm_freq` — live, in a group of its own.

## 10. Decision log

- **`fixed_string`: array named `chars`, `data()`/`size()` as functions.**
  Frees the two names for member functions, which makes the type a valid
  `static_assert` message: a failed compile-time lookup can then print
  `Parameter 'motor.p' is not in the schema` in the diagnostic line itself.
- **No implicit conversion from `fixed_string` to `string_view`.** Template
  parameter objects have static storage, so the intended use is safe either
  way, but the type is general-purpose and an implicit view turns any
  temporary into a dangling one.
- **`operator+` is `consteval`**, with literal overloads on both sides, so
  message building reads as `"unknown parameter '" + Name + "'"` and cannot
  leak into run-time string building.
- **`write`, not `program`.** `read`/`write` is the natural pair and matches
  the driver vocabulary already in the tree; the asymmetry that `program`
  hinted at is carried by `needs_erase` and the concept comment instead.
- **Tests report failure by returning `false`, not `assert()`.** Under
  `NDEBUG` an assert inside a constexpr test evaluates to nothing and the
  case passes silently.
- **Four scalars, not the object dictionary's eight.** `bool`, `int32`,
  `uint32`, `float`. Narrow integers buy nothing in a four-byte cell and
  would add a code path through every layer; a parameter that wants one is
  better modelled as `int32`. Widening the set later breaks nothing.
- **Wrapper types are recognised by a `value_of` overload**, which
  argument-dependent lookup finds, plus `value_type` + explicit constructor,
  not by naming `units::named_unit` and `emb::clamped`. `settings/value.hpp`
  therefore depends on neither header, and `emb::clamped` parameters come
  for free — which lets the application schema declare `unsigned_pu_f32`
  directly and drop the wrapping that `read_*_config()` did by hand. A free
  function rather than a `value()` member makes wrapping an explicit opt-in
  and lets a wrapper keep its number in a public field, as a structural
  type must.
- **`from_cell` is total, `from_value` is not.** A cell read back from
  storage may hold any bit pattern and must yield a value rather than a
  trap: a bool is any-non-zero rather than a `bit_cast`, and a float may
  come back NaN — which the range check then rejects, since NaN compares
  false against both bounds. A protocol write carrying the wrong type, by
  contrast, is rejected outright.
- **Cells are ordered under their type tag**, never as raw words: as int32
  the cell `0xFFFFFFFB` is -5 and belongs in [-10, 10], while as a word it
  is above every positive bound.
- **No per-parameter `validate` hook.** Bounds cover what a single value can
  be judged on; anything else is a relation between parameters. That check
  belonged to the owner's `configure()`, which sees the whole config
  struct, until applying on command moved it before the change; since
  2026-10-17 it is a rule over the parameters it relates (§4). A type-erased
  hook would also need a thunk per parameter for no gain.
- ~~**`group_id` takes any scoped enum with a one-byte underlying type.**~~
  Superseded on 2026-10-17 by "Groups gave way to rules and to configs
  compared": there are no groups. The set of groups belongs to the product,
  and the runtime table can only hold a number; requiring `: std::uint8_t`
  rather than truncating turns an enum that does not fit into a compile
  error.
- **`apply_policy` defaults to `on_restart`.** Opting into live application
  is a claim about the consuming code; a parameter whose policy was never
  considered should cost a restart rather than silently take effect halfway
  through a control cycle.
- **A bool cell holds 0 or 1.** `less_equal` compares bool cells as words,
  so a cell holding anything else is out of range and falls back to the
  default — even though `from_cell` would read it as true. Totality and
  range checking are separate mechanisms and both are wanted.
- **The image holds no atomics.** It is plain data with typed and erased
  access, which keeps it usable in constant expressions — and therefore
  testable the way everything else in the library is. Since 2026-10-05 the
  section that holds it has none either: a call to a non-const member must
  not overlap any other call, while const calls may overlap each other.
  Sharing one between contexts is the application's business. A section
  can no longer be called from an interrupt handler, which nothing did: an
  interrupt with a value to write hands it to a task, as the inverter's
  drive does with a hall calibration.
- ~~**`take()` instead of `changed()` + `acknowledge()`.**~~ Superseded on
  2026-10-16 by "Application is a transaction on command". Check-and-record
  in one operation cannot drop a change that arrives between the two calls.
- ~~**A write that leaves a cell as it was reports no change**~~
  (`std::nullopt`). Superseded on 2026-10-05 by "Unapplied is a difference
  from the values applied, not a record of writes": a write reports only
  an error now, and one that leaves a cell as it was leaves every
  difference as it was, so a profile written whole, or a restore of
  defaults that were already there, still owes nothing.
- ~~**`pending_changes` is templated on its word type.**~~ Superseded on
  2026-10-05 by "Unapplied is a difference from the values applied, not a
  record of writes": with the masks gone there is no word to choose, and
  the tests check the section as the target runs it, bar the medium.
- **Unapplied is a difference from the values applied, not a record of
  writes.** Next to the image and the values stored, the section keeps a
  third image: the values applied, from which the application last built its
  derived state. A parameter is unapplied while its cell differs from the
  applied one, compared by encoding as `unsaved()` compares, and
  `restart_required()` is `unapplied(apply_policy::on_restart)`. `set`,
  `set_at`, `restore_default_at` and `restore_all_defaults` change the image
  only. `take(group, up_to)` refuses a group in which a value under a policy
  stricter than `up_to` differs, returns `false` for one in which nothing
  differs, and otherwise records every value of the group as applied, for
  the caller to rebuild the group from the image. `load` records what it
  restored, since the application builds its state after the load; `save`
  and `wipe` leave the values applied alone, since a restart owed before a
  save is owed after it. The masks this replaced, one per apply policy,
  recorded writes: a write that changed a cell marked its group under the
  parameter's policy, and the mark stayed until a `take` cleared it, so a
  value written and put back stayed marked. An `on_restart` parameter put
  back kept `restart_required` and the unapplied warning lit until the
  restart, and its mark held every later `live` or `on_safe_state` change of
  its group until then. An `on_safe_state` parameter written while the drive
  ran and put back kept the warning lit and the live changes of its group
  waiting until the drive stopped, and then had the group rebuilt for
  nothing. Now the flags follow the values: a value put back before it was
  applied owes nothing. The price is one image more — four bytes per
  parameter, less the twelve bytes the masks took — which a
  constant-initialized section pays in RAM and again in the flash that holds
  its initial values. Every `take` costs a pass over the cells. An owner
  whose `configure()` refuses the values still has them recorded as applied,
  as the masks had them cleared. Any one-byte group identifier works now,
  where the masks held 32 groups. Rejected: applied cells kept only for the
  parameters that are not live, with a mask for the live ones — two
  mechanisms to save some 120 bytes; a change counter per group and policy,
  which cannot see a value put back; a hash of the cells of each group,
  which can collide and cannot say which parameter differs; and a defaulted
  `operator==` on the image, which libstdc++ lowers to `__builtin_memcmp` on
  a `std::array` while newlib-nano's `memcmp` compares a byte at a time, so
  the comparison stays a loop over the cells, a word at a time.
- ~~**`adopt` records a value its owner has applied itself.**~~ Superseded
  on 2026-10-16 by "A calibration is requested, not adopted". `adopt<Name>`
  writes like `set<Name>` and records the value as applied too. The hall
  sensor applies a calibration result in the timebase interrupt, and the
  drive's task records the twelve angles with `adopt` before it takes group
  `hall`: a `take` that another value of the group calls for still rebuilds
  the sensor from the image, so the angles have to be there first. Written
  with `set`, the angles would wait for a `take` to rebuild a sensor that
  already holds them. Worse, when the group cannot be taken as the
  calibration finishes — the drive already started again, or a stricter
  value of the group differs — an operator who writes the old angles back
  leaves the image saying the old calibration while the sensor keeps the
  new one, and once nothing else differs, no flag says so.
- **The erased accessors are spelled apart from the typed ones** —
  `get_at`/`set_at` by index, `get`/`set` by declaration, `cell_at`/
  `assign_cell` for raw cells. Not merely different addressing: the
  by-index path enforces `writable` and the typed path does not, and one
  name for two promises
  hides that at every call site.
- **`writable` gates the erased path only.** A parameter closed to a
  protocol — a factory calibration — must still be writable by the code
  that owns it, and that code goes through the typed path. The erased path
  includes `restore_default_at`: what a protocol may not write, it may not
  reset either.
- **A decode either loads everything or touches nothing.** An invalid
  record leaves the image exactly as it was, so a store can try the other
  slot and only then fall back to defaults. A valid one starts from the
  defaults, so a parameter the record does not carry comes up defined
  rather than keeping whatever the image held.
- **The load report counts what happened** — stored, loaded, unknown,
  rejected, missing, plus whether the schema matched. The old stack could
  not tell "the parameter did not read" from "the parameter is like that".
  A refused cell counts as rejected and not also as missing: a value
  carried and refused points at a range that moved under an old record, a
  value never carried at a schema that grew.
- **CRC-32 is computed a bit at a time.** A table would cost a kilobyte of
  flash, and when the CRC ran twice a boot it would have saved
  microseconds. Since every call checks every whole record ("Only whole
  records count"), it runs some 130 to 260 times a load or a save once the
  product's flash has gone round its ring, about 0.1 ms each — most of the
  16 to 31 ms a load now takes, and still not worth a kilobyte. If it ever
  is, a table of sixteen words read a nibble at a time gives the same CRC
  about three times faster for 64 bytes, and costs less in constant
  expressions too, where the CRC is most of what the tests spend.
- ~~**Every save asks which slot it takes.**~~ Superseded on 2026-10-03 by
  "A save writes only erased bytes, FRAM included": a save reads every
  slot, not only the one a position names, and there is no position left
  to name one.
- ~~**A restart takes its sequence number from the newest header and its
  position from the newest whole record.**~~ Superseded on 2026-10-03 by
  "Only whole records count" and "Sequence numbers follow the ring": the
  slot and the number both follow the newest whole record now, and a
  header with nothing whole behind it, torn or rotted, moves neither.
- ~~**A load reads every header once.**~~ Superseded on 2026-10-03 by "Only
  whole records count": a load, a save and a wipe each read every slot
  whole, once, and check every record's CRC.
- **The store's buffer is a slot, not a record.** A firmware that declared
  more parameters wrote a longer record, and refusing to read it would
  silently discard the settings of anyone downgrading. The RAM cost is
  `slot_capacity`, which is the application's own number — the same one it
  reserved on the medium.
- **A failed save says which step failed** — scan, erase, body, commit or
  verify — and carries the medium's own error code, except where there is
  no code to carry and the absence is the diagnosis: a scan that read
  every slot and found that the record would not come out newest, and a
  write that went through but did not read back. A save that fails at the
  scan has written nothing; on FRAM, `erase` is the fill of the slot.
- ~~**The slot and the sequence number are both spent before the first
  write**~~, whether or not the attempt succeeds. Superseded on 2026-10-03
  by "Sequence numbers follow the ring": nothing is spent in RAM. An
  attempt that left nothing costs nothing, and one that left a record or
  debris is found on the medium by the next save.
- ~~**A save never takes the slot of the newest record the store knows
  of**~~, nor enters its block on a medium that must be erased. Superseded
  on 2026-10-03 by "A save writes only erased bytes, FRAM included": the
  store knows of no record between calls; every save finds the newest one
  on the medium and writes after it, never over it, and never erases its
  block.
- ~~**A load that restores nothing positions itself from the headers.**~~
  Superseded on 2026-10-03 by "The store keeps nothing about the medium"
  and "A save refuses a section it cannot read whole": there is no
  position to set, and after a read failure a save writes only once every
  slot reads.
- ~~**A save before the first load surveys the headers.**~~ Superseded on
  2026-10-03 by "The store keeps nothing about the medium": every save
  reads the whole section first and checks records rather than headers,
  so a save before the first load is like any other.
- **The store keeps nothing about the medium.** Between calls it holds the
  medium and a buffer, nothing else. Every load, save and wipe reads every
  slot whole, sorts each into erased, a whole record with its sequence
  number, or anything else, and decides from that table alone, by pure
  functions. The tests check those on every arrangement of erased, whole and
  other slots on five rings of two to six slots, the whole records numbered
  as this store numbers them, and on a ring of eight slots in blocks of
  four, where only the slots after the newest record in its block take every
  class. A restart, a save before the first load and a save after a failed
  one are therefore one case. The store it replaced carried a position, a
  sequence number, the slot of the newest record it knew of and whether it
  had surveyed the headers, and each of its special cases — the struck
  entries above — patched a way that knowledge could part from the medium: a
  restart, a failed save, a run of them, a save before any load, a load that
  restored nothing. Each patch was argued sound, and twice the argument was
  wrong; a model of the store that tried every fault on every path found six
  more ways to lose a save, two of them from a single rotted bit (the
  entries below). Which slots are erased, which record is newest and what to
  number next are facts about the medium, and reading them again costs less
  than proving a copy of them right. The price is the pass: on the product's
  flash, 256 reads of a kilobyte and a CRC of every whole record make a load
  16 to 31 ms and a save 25 to 40 ms, against 0.2 and 8 before; on FRAM a
  load takes 12 ms at 2 MHz and 49 at 500 kHz, a save 16 and 66. The product
  loads once at startup and saves only while the drive stands still. The
  table takes about a kilobyte of stack in the flash geometry, as the header
  map did.
- **Only whole records count.** A slot is a record only if `check_record`
  accepts it: the header's magic and format, a size that fits the slot,
  the footer's magic and the CRC. A header with nothing whole behind it —
  a torn save's, a rotted one, another section's leftovers — counts for
  nothing: it neither names the newest record nor numbers the next. The
  old store let headers do both, to spare a CRC per slot, and lost saves
  through them. A save after a load the medium refused trusted the header
  of a torn save, which on two FRAM slots put it over the last good record
  and on flash, with no rot at all, could erase that record's block; and a
  number taken from a header whose top bit had rotted moved the count by
  2^31, after which a save that had succeeded could lose, after a restart,
  to a record 45 saves older. `decode_record` calls `check_record` first,
  so the pass and the load agree on what is whole. The pass reuses the
  buffer, so a load reads the newest record again to decode it. A refused
  read there is retried once, since one refusal would otherwise send the
  load to an older record or the defaults with the newest just read whole,
  and a record that reads back other than the pass saw it drops out like
  any other. Decoding during the pass would save that read at the price of
  a second path through the choice of the newest.
- **The newest record is the one newer than all others.** `seq_newer` is
  modular, which lets the counter wrap, but it is not an ordering: it is
  not transitive, and of two numbers 2^31 apart neither is newer. The old
  load took a running maximum under it, which on numbers that far apart
  depends on the order of the slots: on the product's flash, 300 saves and
  one rotted top bit in slot 0 made it restore the 256th, and the next
  save erased block 0, holding the 257th to the 300th. The newest record
  is now the whole record newer than every other whole one; of two with
  one number, the one in the lower slot, as before. On a medium this store
  wrote, such a record always exists, since the numbers of its whole
  records lie within a ring's length of each other ("Sequence numbers
  follow the ring"), where the relation is a strict order. Where none is
  newer than all the others, the highest number taken unsigned wins. Only
  the old store left such media, by numbering above a header whose top bit
  had rotted — NOR loses programmed bits, and a sequence number is mostly
  zeros — so the records with the top bit set are the ones it wrote after
  the rot, and the highest of them is the newest. Without rot, the rule
  restores what the old load restored.
- **A save writes only erased bytes, FRAM included.** Slots form blocks on
  every medium, a block being what a save erases on entering it: a sector
  on flash, one slot on FRAM, where erasing fills the slot with
  `erased_value`. A save takes the first slot after the newest record, in
  its block, that reads erased; if none is left, it erases the next block
  and takes its first slot. It never writes the newest record's slot nor
  erases its block, and it enters any other block only through a whole
  erase, so a block that a cut erase left half done is erased again rather
  than written into. A tear then leaves the new record's prefix over
  erased bytes and never splices it onto an old record. On FRAM such a
  splice could bring one back: once the numbering restarted — after a wipe
  cut short, or with every record corrupted — a new record could carry the
  header of an old one still in its slot, and a tear right after the bytes
  the old one had lost left it whole again, numbered above the last
  successful save. The fill costs a kilobyte written per save, 4 ms at
  2 MHz and 16 at 500 kHz, and buys one rule for every medium: the store
  no longer asks `needs_erase`. Entering a block erases it even if it
  reads erased, which rules out cells an interrupted erase left weak, at
  the cost of one needless erase per block on a fresh medium and as many
  after every wipe — the wipe erases every block, and the saves after it
  erase each again on entry: two sector erases each time on the product's
  flash. Inside a block a refused write or a medium that keeps nothing
  costs nothing, since the next save finds the same slot erased; at the
  start of a block every failed attempt erases the block again.
- **Sequence numbers follow the ring.** A record is numbered with the
  newest record's number plus the distance from that record's slot to its
  own along the ring, and the first record of an empty section with one.
  A number is thus the record's place on the ring unrolled since the
  section was last empty, so on flash the number over `slots_per_block` —
  `erase_cycles` under `3000h` — again counts the blocks the saves have
  filled, about the erases. Nothing is spent on an attempt: one that left
  nothing costs nothing, one that left a record is found by the next pass
  and numbered above, and one that left debris costs the slot the debris
  occupies. The old store spent a number and a slot on every attempt,
  because a save that failed may still have landed and reusing its number
  would put two records on one generation; the pass now sees what landed.
  On a medium this store wrote, the numbers of whole records differ by
  their distance on the ring, less than its length and far inside the
  2^31 within which `seq_newer` orders. Counting the distance rather than
  adding one also gives every slot its own number from the same newest
  record: the debris of an attempt that later reads whole — a write cut on
  marginal cells can — never shares a number with the save after it, nor
  outranks it.
- **A save refuses a section it cannot read whole.** If the medium refuses
  a read of any slot, the save writes nothing and fails with
  `save_stage::scan` and the medium's error. The slot it could not read
  may hold the newest record, and every decision hangs on that record: a
  save without it may number below it, write over it or erase its block.
  The old store took what it could not read for absent: a save after a
  load that read nothing took the section for empty, numbered its record
  one and erased block 0, and a load that missed some headers numbered the
  next record below records it could not see. A load still restores the
  newest record among those it read and reports that a read failed, and a
  wipe erases whether it read or not (below). On the product's media a
  refused read is a failing FRAM bus — the internal flash refuses none —
  so a save goes through again once the bus does.
- **A save checks before writing that the next load would restore it.** It
  applies its plan to the table the pass built — the block erased if it is
  to be, the new record in its slot — and requires the newest record of
  the result to be the new one; otherwise it writes nothing and fails with
  `save_stage::scan`, without a cause. On a medium this store wrote the
  check never fails, since the new record is numbered above every whole
  record within a ring's length. It is there for the media the old store
  left after a rotted top bit, where whole records can lie 2^31 apart and
  a record numbered from the newest need not come out newest: without the
  check such a save would succeed and the next load restore something
  older. With it, success means that the next load that reads the section
  restores this record, on every medium. A wipe ends the refusals, and
  since a wipe leaves the image as it was, a save right after it keeps the
  values in use.
- **The read-back compares the CRC it wrote.** After the footer, a save
  reads the record back and requires it whole, with its own sequence
  number and the CRC it encoded. The old read-back only asked whether what
  it read was a consistent record with that number: on two FRAM slots,
  after a load that could not read the newest header, a save numbered
  itself like that record, wrote over it into a memory that kept nothing,
  read the record back whole under its own number and reported success;
  the next load restored the old values. The CRC makes the check one of
  identity — another record under the same number has another CRC unless
  it holds the same values. A read-back the medium refuses fails with its
  error; one that reads something else fails without a cause, the
  signature of a memory that does not keep what it accepts.
- **A wipe erases the newest record's block last.** It erases every block
  in ring order, starting after the newest record's, so a wipe cut short
  leaves the newest record or nothing: the blocks erased first hold only
  older records, and in the last one the records before the newest go
  before it, as long as an erase cut short clears its range from the
  start. Erasing in address order, as before, could leave an older record
  standing alone — up to a block of saves older — and the next load
  restored it. A tombstone was rejected: a record of defaults written into
  a freshly erased block of its own, then every other block erased, then
  that block. It costs one more sector erase, about two seconds, needs a
  section that reads and writes, and a wipe cut short can leave it whole,
  a valid record of defaults that a load cannot tell from real settings.
  A pass to confirm the erase was rejected too: it would need an error of
  its own from `wipe`, and the application to handle it. A wipe reads
  only to find that order and erases even what it could not read: it is
  the last resort for a section that refuses saves, and must not refuse in
  turn. Such a wipe guesses the order from what it read and promises
  nothing about a cut.
- **Tests follow the in-tree convention** (`emb/<module>/test/*_test.cpp`,
  anonymous namespace, `static_assert` only): they cost compile time and
  contribute no symbols to the image.
- **Settings objects are rows with a binding, not a generated section.** A
  row names its parameter with `od_settings<...>::rw<declaration>` inside
  the ordinary table, so settings objects and the others share one table, one
  sort and one set of checks. A separate section would have needed a merge
  step and a second place where keys live.
- **Coverage through a catalog each row carries.** The bridge cannot see the
  dictionary it ends up in, so its rows point at the schema's names and
  `expose` flags, and the dictionary checks every catalog it finds. The
  price is a blind spot: a dictionary without a single settings row is not
  checked, since there is nothing to find the catalog through.
- **The row states the type the schema already fixes.** A host's table
  (ucan-monitor) is to be generated by a text parser, and the parser cannot
  evaluate the schema's expressions (`U::rpm_f32{100.0f}`, a board's
  frequency). The type column is checked against the binding, so the
  duplication cannot drift. Bounds and defaults stay out of the row: some
  depend on the build (`drive.pwm_freq` differs between debug and release)
  or on the type (`pu`, `spu`), and a copy in the row would be wrong for
  some build.
- **A parameter is named by its declaration, not by its name** (2026-10-15).
  The typed access and a dictionary row take the `constexpr` variable that
  holds the declaration, `get<params::motor::p>()` and
  `rw<params::motor::p>`, and `parameter` checks that the schema holds that
  declaration, of the same type and descriptor. A misspelt key is then an
  unknown identifier, which an editor completes and finds the uses of, and
  the key leads to the parameter's type, default and bounds in one step.
  The name stays the key of the stored value and of the catalog, so
  renaming the variable keeps what is stored and changing the name loses
  it, as before. Both keys side by side were rejected: `parameter` and `rw`
  are a class and a variable template, which cannot be overloaded on the
  kind of their parameter, and a key type taking either would keep two
  spellings for good. A declaration is taken as `auto& Decl` constrained by
  `some_declaration<decltype(Decl)>`: Clang rejects the shorter
  `some_declaration auto& Decl`.

- **Application is a transaction on command** (2026-10-16). `apply`,
  `apply_group` and `apply_all` replace `take` and the periodic task that
  called it. A transaction checks the values that every group of a change
  would take, then admits each group under the applier's lock, and only
  then records the values and configures the groups, so a refusal leaves
  nothing half applied. The check is pure and runs before the lock; the
  admission reads the state and runs under it, so the state it read holds
  through the configure. Rejected: a task that applies what it can, which
  leaves the client guessing when a value took effect; and an admission per
  parameter, which would have the schema state what the application knows
  better.
- ~~**Two policies; the groups say when.**~~ Superseded on 2026-10-17 by
  "Groups gave way to rules and to configs compared": the applier says
  when, from what a change touches. `live` and `on_safe_state` became
  `on_command`: when a change may take effect is the application's to say,
  group by group, through `admit`, and a group is split where its values
  may take effect at different times. Rejected: keeping `on_safe_state`,
  whose meaning differed from one parameter to the next.
- **Configs are built from the values applied.** A config read from the
  values requested carried every value that waited in its group, which
  forced a group to be applied whole and let a value that had to wait
  freeze its group. Read from the values applied, a config carries exactly
  what was applied, so groups that feed one config may be applied apart.
- **The applier is passed to each call, not kept by the section.** At
  startup there is no applier yet — it holds the drive, built from the
  values loaded — and a section that kept one would have a state without
  it and a template parameter naming the application. The checks are a
  concept of their own, `some_checker`, since a save and a startup need them
  with nothing to configure.
- **`configure` cannot fail.** Whatever can refuse is asked before anything
  changes, by `check` or `admit`; a configure that failed after the values
  were recorded would leave them applied but not in effect.
- **A save keeps what is applied or checked.** It refuses while a value
  applied on command waits, and checks the values that wait for a restart,
  so the next startup loads what ran or what was checked. Rejected: saving
  the values applied in place of those requested, which would store
  something other than what the client sees.
- **`restart_required` counts what the next startup would change.** An
  `on_restart` value stored that differs from the one applied owes a
  restart as much as a requested one does: a save followed by a discard
  would otherwise hide a restart that loads the value saved.
- ~~**A group invalid at startup falls back to its defaults.**~~ Superseded
  on 2026-10-17 by "A rule names its inputs, and reads nothing else": what
  falls back are the inputs of a broken rule. The values loaded stay
  requested, so the client sees what was refused and can correct it, and a
  save refuses until it does. The defaults of each group pass its check,
  which the application holds with a `static_assert`.
- **A calibration is requested, not adopted.** A hall calibration reached
  the sensor from the interrupt and the settings through `adopt`, behind
  the take. It goes through a mailbox to the main loop now, which requests
  the angles for an operator to apply; `adopt` is gone.
- **Records, not single objects** (2026-10-16). A parameter shows its value
  requested, applied and stored, its default, bounds, policy and group, and
  the commands that apply and discard it, as members of one record, and a
  group as another. A record is one row whose layout of fields every record
  of its kind shares, so the dictionary keeps one entry per record and the
  checks run on rows. 1011h:04 went with it: the default is a member now.
- **Groups gave way to rules and to configs compared** (2026-10-17). A
  group decided three things for a change: what to check, whether the
  change may take effect now, and what to rebuild. With the configs built
  from the values applied and a change applied whole, the application reads
  all three off the values: the applier compares what it builds from the
  values applied before and after a change, rebuilds what differs and
  admits the change by what it touches, and a check is a set of rules over
  the parameters each names. The group of a parameter had to agree with
  the reader of its config, and nothing checked it: a parameter read into
  one config but declared in another's group would show its new value as
  applied while the config kept the old one. The records of the groups,
  08h of a parameter's record and 3001h:04 went with them, and so did the
  link and the second argument of a record in the dictionary, which nothing
  else used. Rejected: groups kept inside and dropped from the bus only,
  which kept the duplication and the groups of one parameter that existed
  to be admitted apart (`phase_order`, `autotune` in the inverter); and the
  number of the broken rule in place of 3001h:04, which would have made the
  order of the rules a contract.
- **A rule names its inputs, and reads nothing else** (2026-10-17).
  `inputs<schema, Decls...>` shows a rule its parameters and deletes `get`
  of any other, so the list cannot miss a parameter the rule reads, and
  the defaults of the inputs mend a rule that breaks, since they pass it.
  What falls back is the inputs of the rule, not the config it guards:
  falling back everything `drive_config` reads would reset the phase order
  with a threshold. Rejected: the whole image, which would build `motor.p`
  and the current scales from defaults that only a restart replaces;
  inputs found by changing each parameter at compile time, which sees only
  what changes the result near the defaults; and rules over whole configs,
  whose inputs would be the whole config.
- **A fallback runs pass by pass to a fixed point** (2026-10-17). Each pass
  checks every rule before it falls back any, so a rule that would mend
  another by going first does not decide by its place in the list, and a
  rule falls back at most once, so there are at most as many passes as
  rules. Rejected: falling back a rule as soon as it breaks, whose outcome
  depends on the order of the list; and forbidding rules to share inputs,
  which would merge them and coarsen both the fallback and the refusal.
- **One parameter still applies alone** (2026-10-17). It is the one way to
  apply part of what waits, e.g. a gain while a change that needs the drive
  to stand stays requested. Each step is checked as the values applied with
  that one value taken in, so values a rule relates may need an order, and
  the twelve hall angles of a calibration go together through 3001h:01.
- **What may change while the drive runs is listed; anything else waits**
  (2026-10-19). The inverter named what needed the drive to stand — the
  phase order, the machine, autotune, the hall sensor — and static
  assertions pinned that list, but only for the parameters they named: a
  field added to `drive_config` that needed the drive to stand would have
  taken effect on the run. Each config now lists its live fields in
  `needs_standstill`, and `change_between` reads every config through it, so
  a new field waits until it is listed and a new config does not compile
  until it says. A mistake now refuses a change on the run, which the client
  sees, rather than letting one through, and the static assertions went with
  the risk they guarded against. The live fields are blanked in both configs
  before they compare. Rejected: copying them over from one config to the
  other, which leaves a float to compare with itself, which the compiler
  keeps for the sake of NaN — 678 bytes of flash more.
- **The calibration of the hall sensor is a config of its own**
  (2026-10-19). `emb::hall::angle_sensor_config` held the time constant of
  the speed filter beside the calibration, so the time constant waited for
  the drive to stand only because it shared a struct with the angles, and
  the rule of the angles had to be handed a calibration built apart from
  the config. The config keeps the time constant alone, a place for the
  sensor's tuning to come; the sensor takes the calibration beside it and
  applies one through `apply_calibration`. `hall.speed_tc` may change while
  the drive runs, like the filters of the temperatures, and the rule and the
  drive read the calibration with one reader.
- **The scalar of a parameter is its `scalar_kind`, its encoding a `cell`**
  (2026-10-19). The tag was `value_type`, while the same header named a
  wrapper's scalar `T::value_type`; the descriptor's `type` named the scalar
  while `declaration<T>::type` and `parameter::type` named the C++ type; and
  the four-byte encoding was `raw_value`, converted by `to_raw` and
  `from_raw`, while every comment called it a cell. `scalar_kind`,
  `kind_of`, `held_kind` and `descriptor::kind` now name the scalar, and
  `cell`, `to_cell`, `from_cell` and `image::cell_at` the encoding, so
  "type" in `emb::settings` is the C++ type alone. The section's image of
  the values requested is `requested()`, beside `applied()` and `stored()`,
  and `image::assign_defaults` writes without checks, as `assign_cell` does.
  Records keep matching: the identifier hashes the value of the enumerator,
  not its name.
- **A command is a write-only `uint32`** (2026-10-19). The dictionary had a
  type code of its own for commands, `exec`, which CiA 301 does not know,
  and which travelled as a `uint32` anyway; 1010h and 1011h, which now take
  a signature, carry a `uint32` value outright. `od_exec` makes a
  write-only `uint32` object, the members 08h and 09h of a parameter's
  record are such objects, and the rule that an `exec` object be
  write-only went with the type: the builder makes it so. `string` stayed
  for the moment, the server reading it four bytes at a time; it went the
  same day ("A string is an array of its characters").
- **A string is an array of its characters** (2026-10-19). A string was
  read four bytes at a time, and a cursor in the server remembered which
  word came next: the one state the server kept between frames, which a
  client mixing reads reset and no standard client knew. A string is an
  array now: subindex 00h reads its length, and each subindex after it one
  `uint8` character, up to 254. `od_string` builds it from the reader that
  `od_text` took, and `find` serves the subindices of an array as it serves
  those of a record, with the element's position as its argument. The type
  `string` went, and `od_alternative_of` with it: the type codes are the
  alternatives of `od_value`, one to one. Rejected: four characters to an
  element, which keeps the traffic of before but makes the elements words
  of text, and a type `char`, which CiA 301 does not have.
- **The dictionary has a fingerprint** (2026-10-20). A host decodes what
  the inverter sends by a table generated from `od.cpp`, and nothing told
  a stale table from a fresh one: addresses and types moved three times in
  two days. `make_dictionary` hashes, with FNV-1a, a text that describes
  every row in the order of keys, its key, names, type, kind and access,
  and every field of a record's layout, and the inverter shows the hash at
  2006h:00, `dictionary_id`. A host that writes the same text from its
  table compares the two. The unit is left out of the text: it changes how
  a value reads, not where it is or how it travels.
- **A list of values is an array** (2026-10-20). The temperatures of the
  power module's and the motor's sensors, the external inputs and the
  probe channels were objects at subindices of 5000h, one each, and where
  a board had fewer sensors than subindices, a template reader answered
  06020000h: a host could not learn how many sensors a board had.
  `od_ro_array` binds a reader that returns a `std::array` as an array:
  subindex 00h reads its length, which the type gives, and each subindex
  after it one read-only element. The inverter serves its four lists so,
  at 5001h–5004h. Rejected: a reader of one element by its position, with
  the length written in the builder, which could disagree with the source
  and let the element's handler read past its end.
- **The dictionary serves subindex 00h of an index with members**
  (2026-10-20). An index of several objects from 01h on, such as 3000h or
  5000h, answered 06090011h at 00h, where CiA 301 puts the highest
  subindex of an object with members, and a standard client reads it
  first. `make_dictionary` adds an entry at 00h to each index whose
  objects all lie past it: a `const` `uint8` that reads the subindex of
  the last. A row at 00h that shares its index with others breaks a new
  rule, T25, so the place is never taken. The fingerprint does not
  describe the entries added: they follow from the rows. Rejected: a row
  at 00h written by hand, which would have to follow every object added
  to the index.
- **The fingerprint is the inverter's first object** (2026-10-20). A host
  reads `dictionary_id` before it knows which table to decode the
  inverter by, so its address must not move when the table does. It
  moved from 2006h to 2000h, the first index of the manufacturer's area;
  `device_name` and `serial_number`, which tell one inverter from
  another, follow it at 2001h and 2002h, and the rest of the information
  after them.
- **The inverter's objects left the area of CiA 301** (2026-10-19). Its
  dictionary kept objects in 1000h–1FFFh that the standard defines
  otherwise: strings at 1008h–100Ah, two of them at subindices a VAR does
  not have, a serial number at 1018h without the identity's other
  members, and at 1010h:03 and 1011h:03 a save with a reset and an erase,
  where CiA 301 puts the application's parameters. They moved to the area
  of the manufacturer, which the standard leaves to the device: the
  information at 2000h–2005h, the work with the settings at 2100h–2102h,
  the control at 2200h–2206h, each command of which has an index of its
  own. The area of the standard holds nothing until the inverter
  implements an object of it as the standard says. The names stay, so a
  host that finds objects by name finds them once it reads the new table.

## 10a. What changed for an operator

The switch-over changed two behaviours on purpose, and both are visible
from a CANopen tool:

- **A write no longer persists by itself.** It lands in the image; `1010h`
  puts it on the medium as one record. That is what buys atomicity — a
  calibration reaches the memory whole or not at all — and it is what the
  object is for. Saving on every write was considered and rejected: it
  writes a whole record per parameter, which is harmless on FRAM and not on
  flash. A debounced save (a quiet period after the last write) belongs
  with the reconfiguration points in step 15, which did not bring one: a
  save still takes `1010h`.
- **A value outside a parameter's bounds is refused**, with
  `value_range_exceeded`, where before it was silently clamped by the
  wrapper the config reader applied. The operator now learns.

The rewrite of the store on 2026-10-03 (§10) changed three more:

- **`sequence` and `erase_cycles` follow the ring.** A record's number is
  the newest record's plus the slots from it to the new one, so `sequence`
  (`3000h:05`) is the newest record's place on the ring unrolled since the
  section was last empty, and on flash `erase_cycles` (`3000h:06`), its
  quotient by `slots_per_block`, again counts the blocks the saves have
  filled — about the erases. A save that failed and left nothing costs no
  number; one that tore makes the next record's number skip the slot it
  tore in. Debris and rotted headers move neither count, where a rotted
  top bit used to add 2^31. `sequence` follows the medium: a load sets it
  to the number of the record it restored, or zero if none; a save that
  succeeds, to its record's; a wipe that succeeds, to zero. A failed save
  leaves it as it was, where it used to count the attempt. Right after the
  update both can read lower than the old firmware showed, since it
  numbered above the headers of torn saves and counted every failed
  attempt.
- **A save refuses a medium it cannot read.** If any slot fails to read,
  `1010h` answers `data_store_error` and `nvm_write_error` is raised, as
  for any failed save, but nothing was written: the slot that did not read
  may hold the newest record, which the old firmware, writing blind, could
  erase or number below. The save goes through once the medium reads
  again. On a medium the old firmware numbered above a rotted header, a
  save can be refused the same way with every read succeeding, when no
  record it could write would come out newest; erasing all parameters
  (`1011h:03`) clears that, and the next `1010h` stores the values in use.
- **A save on FRAM fills its slot before writing the record**, and every
  load and save reads the whole section first. On FRAM a save takes about
  16 ms on the rev_a's 2 MHz bus and 66 on the miniboard's 500 kHz, where
  it took 4 and 16, and a load 12 and 49, where it took 2 and 9. On the
  product's flash a load takes 16 to 31 ms and a save 25 to 40, where they
  took 0.2 and 8.

Counting as unapplied what differs from the values applied, on 2026-10-05
(§10), changed three more:

- **`restart_required` and `unapplied` follow the values.** `unapplied`
  (`3000h:12`) and the warning `unapplied_changes` stand while any value
  differs from the one applied, `restart_required` (`3000h:11`) and its
  warning while an `on_restart` value does, and each clears once the
  values are applied — at the restart, for `on_restart` — or put back.
  Before, a write kept them lit until its group was applied, even after
  the value was put back: an `on_restart` parameter until the restart,
  holding back every later change of its group, and an `on_safe_state`
  one written while the drive ran until the drive stopped. Put back means
  bit for bit, since cells are compared by their encoding: `-0.0` does not
  put back `+0.0`. A value put back before it was applied rebuilds nothing
  either: put back in group `motor`, it no longer resets the observer, and
  a value set through the `ctl` objects `2001h:03` to `2001h:05`, which
  bypass the image, survives such a pair of writes.
- **`1011h:04` clears them only where the value applied is the default.**
  It restores the default, not the value applied, so it undoes a change
  only of a parameter that runs on its default. For any other, write back
  the value it runs on.
- **`3000h:12` is called `unapplied`**, after the member that answers it,
  where it was `changes_pending`. A tool that finds objects by name needs
  the new one.

Applying on command, on 2026-10-16 (§4, §7), changed more:

- **A write takes effect only when applied.** Writing 01h of a record
  requests the value; 09h of the record, 01h of its group's record, or
  3001h:01 for everything applies it, and the answer of the command is the
  result: 06040043h if a check refuses, 08000022h if the drive cannot take
  the change in its state, 06010000h for a parameter applied only after a
  restart. 0Ah, 02h of the group's record and 3001h:02 discard. 3001h:03
  and 3001h:04 show the abort and the group of the last refusal.
- **The settings moved.** Every parameter is a record at an index of its
  own, from 3100h, in blocks of 20h by category, its category and name as
  before; every group is a record at 3010h-3019h. The gains the current
  controllers work with moved from 3006h:09-0Ch to 0Bh of their records.
- **A save refuses what waits.** 1010h answers 08000022h while a value
  waits to be applied and 06040043h while a value waiting for a restart
  fails its check, so restoring the defaults (1011h:01) takes an
  application before a save. 1011h:01 no longer needs a safe state: it
  only requests. 3000h:13 tells whether a value waits to be saved.
- **1011h:04 is gone**: a client reads 04h of a record and writes it to
  01h.
- **A calibration of the hall sensor waits to be applied.** Its twelve
  angles are requested, and the sensor keeps its own until the group `hall`
  is applied.
- **`restart_required` stays lit after a save and a discard**, until the
  value stored is again the one the machine runs on.
- **A group that fails its check at startup runs on its defaults**, with
  `invalid_config` (`hall_invalid_calibration` for the hall sensor), and
  the values loaded wait to be corrected or discarded.

Dropping the groups, on 2026-10-17 (§4, §7), changed:

- **The records of the groups and 3001h:04 are gone.** 3001h:03 still
  shows the abort of the last refusal; what broke, a client reads off what
  it requested.
- **The members of a parameter's record from 09h moved down by one**: 08h
  applies the parameter, 09h discards it, 0Ah shows the actual value, and
  00h reads 09h, or 0Ah with an actual value.
- **Part of what waits applies one parameter at a time**, by 08h. A rule
  may ask for an order — raising both bounds of the DC window, `ovp_dc`
  first — and a calibration of the hall sensor applies with 3001h:01, the
  twelve angles together.
- **A rule that the values loaded break resets only its inputs** to their
  defaults: `uvp_dc` and `ovp_dc` for the DC window, the two margins for
  the overtemperature warning, `hall.enabled` and the twelve angles for the
  hall sensor, where a group reset all fourteen of its parameters —
  `runout_speed` and the stopping parameters with the thresholds, or
  `hall.speed_tc` with the angles.
- **A cell changed from `+0.0` to `-0.0` is applied without a rebuild**, as
  no number changes; it needs no standstill either.

Cutting the calibration out of the hall sensor's config, on 2026-10-19
(§4), changed:

- **`hall.speed_tc` applies while the drive runs.** Only the twelve angles
  of the calibration wait for the drive to stand with its bridge off.

Checking what a client writes, on 2026-10-19 (§7), changed:

- **1010h and 1011h ask for their signatures**, as CiA 301 does: a save
  (1010h:01, :03) runs only when the word written is "save", 0x65766173,
  and restoring the defaults or erasing (1011h:01, :03) only when it is
  "load", 0x64616F6C. Any other word answers 08000020h, and nothing is
  saved or erased. A client that writes zeros to these objects is refused
  until it writes the signatures.
- **A command is a write-only UNSIGNED32**, as in CiA 301: the dictionary
  has no type `exec` any more, and the type of every command, 1010h,
  1011h, 2000h, 2001h:01, 3001h:01 and :02 and the members 08h and 09h of
  a parameter's record, reads `uint32`. A host that told commands by the
  type `exec` tells them by their write-only access.
- **A write must carry the object's size.** An expedited download that
  tells another size than the object's answers 06070010h; a download that
  is not expedited, which would carry the size of a segmented transfer,
  answers 06010000h.
- **3001h:03 is `last_refusal`** and shows the abort of the last refusal of
  a save as well as of an application: 08000022h while values wait to be
  applied, 06040043h when they fail their rules, 08000020h when the medium
  fails. It showed only the refusals of applications, and of a save only
  the one of its check, under the name `last_failure`.

Moving the objects out of the area of CiA 301, on 2026-10-19 (§7),
changed:

- **The objects left the area of CiA 301** for the area of the
  manufacturer, under the same names:

  | Was | Object | Is |
  |---|---|---|
  | 1008h:00 | `device_name` | 2000h:00 |
  | 1009h:00 | `hardware_version` | 2001h:00 |
  | 100Ah:00 | `firmware_version` | 2002h:00 |
  | 100Ah:01 | `firmware_commitdate` | 2003h:00 |
  | 100Ah:02 | `firmware_branch` | 2004h:00 |
  | 1018h:04 | `serial_number` | 2005h:00 |
  | 3001h:01–03 | `apply_all`, `discard_all`, `last_refusal` | 2100h:01–03 |
  | 1010h:01, :03 | `save_all_parameters`, `save_all_parameters_and_reset` | 2101h:01, :02 |
  | 1011h:01, :03 | `restore_all_default_parameters`, `erase_all_parameters` | 2102h:01, :02 |
  | 2000h:01–03 | `reset_device`, `clear_errors`, `emergency` | 2200h–2202h |
  | 2001h:01, :06–08 | `calibrate_angle_sensor`, `dc_a`, `dc_b`, `dc_c` | 2203h–2206h |

  The signatures stay with the commands that moved from 1010h and 1011h.

Reading text as arrays, on 2026-10-19 (§7), changed:

- **The device's strings are arrays** at 2000h–2004h: 00h reads the
  length, 01h and on one character each, and a subindex past the length
  answers 06090011h. A client that read a string four bytes at a time from
  subindex 00h reads its length there now.

Fingerprinting the dictionary, on 2026-10-20 (§7), changed:

- **2006h:00, `dictionary_id`**, shows the fingerprint of the dictionary:
  the FNV-1a hash of a text that describes its rows, which a host writes
  from its own table to tell whether that table is the inverter's.

Serving the lists of values as arrays, on 2026-10-20 (§7), changed:

- **The lists are arrays**, each at an index of its own, and the objects
  at subindices of 5000h they replace are gone:

  | Was | Objects | Is |
  |---|---|---|
  | 5000h:24–26 | `temp/Tpwr_a`–`Tpwr_c` | 5001h, `temp/Tpwr_sensors` |
  | 5000h:27–2B | `temp/Tmot1`–`Tmot5` | 5002h, `temp/Tmot_sensors` |
  | 5000h:81–84 | `ext/V1`–`V4` | 5003h, `ext/V` |
  | 5000h:F1–FA | `logger/ch0`–`ch9` | 5004h, `logger/ch` |

  00h reads the number of elements: three sensors of the power module and
  five of the motor on rev-a, one of each on the miniboard, four external
  inputs and ten probe channels on both. The first element is at 01h; one
  past the last answers 06090011h, where a sensor the board lacks answered
  06020000h.

Serving subindex 00h of the indices with members, on 2026-10-20 (§7),
changed:

- **Subindex 00h reads the highest subindex** at 2100h (03h), 2101h and
  2102h (02h), 3000h (13h), 3008h (06h), 3009h (02h) and 5000h (79h), as
  a `const` `uint8`. It answered 06090011h.

Telling the build and putting the fingerprint first, on 2026-10-20 (§7),
changed:

- **2007h, `build_type`, and 2008h, `settings_storage`**, read
  "debug" or "release" and "flash" or "fram", as arrays of characters.
- **The information moved** so that the fingerprint comes first, under
  the same names:

  | Object | Was | Is |
  |---|---|---|
  | `dictionary_id` | 2006h:00 | 2000h:00 |
  | `device_name` | 2000h:00 | 2001h:00 |
  | `serial_number` | 2005h:00 | 2002h:00 |
  | `hardware_version` | 2001h:00 | 2003h:00 |
  | `firmware_version` | 2002h:00 | 2004h:00 |
  | `firmware_commitdate` | 2003h:00 | 2005h:00 |
  | `firmware_branch` | 2004h:00 | 2006h:00 |

## 10b. What applying taught us

The first two lessons concern `take()`, which applying on command replaced
on 2026-10-16 (§4); they stay for the record.

- **A group is applied whole, so a group that also waits on something
  stricter must not be applied at all.** Group `drive` holds fifteen live
  protection thresholds next to `drive.phase_swap`, which may only change
  standing still; taking the live part would rebuild `drive_config` from
  values that include the swap. The rule lives inside `take()` rather than
  in its callers: a caller that forgets it gets no diagnostic, only a phase
  swap in a spinning machine. A consequence worth knowing: one parameter
  needing a restart freezes live tuning of its group until the restart or
  until the value is put back, which is the safe direction.
- **`take()` has a precondition**: writes to the parameters of the group
  must not run between `take` and the caller's reading of the image. A
  value written in between reaches the caller whatever its policy — a
  stricter one too, which `take` never saw. No check inside `take` closes
  that window: the take and the reading of the configuration would have to
  be one operation. In the inverter the precondition holds by
  construction: every call on the section runs in the main loop — SDO in
  `can_server.run()`, the drive's task, the trouble task — so nothing
  writes between a `take` and the reading after it.
- **Reconfiguration is not atomic against the current loop.** The task
  tick masks the timebase and below; the ADC and PWM interrupts (priority
  0 and 2) still preempt. No value tears — every one is a word — and each
  group's derived state tolerates a single control step built from a mix of
  old and new coefficients. The one structure that would not, the angle
  sensor's sector maps, is only ever rebuilt in a state where no current
  flows, which is what its `on_safe_state` policy is for. Masking the
  timebase does a second job: the drive's state machine moves in the
  timebase interrupt, so the state `up_to` was read from holds until the
  rebuild ends.
- **`isense.zero_drift_th` went back to `on_restart`.** Nothing can reach
  the zero-drift calibrator to tell it otherwise: it is not owned by the
  drive and has no periodic task of its own. Declaring it live would have
  been a promise the code does not keep.

- **Building from the values applied is what lets groups apply apart.**
  Rebuilt from the values requested, a config carried every value waiting
  in it, which is why a group had to be taken whole and why one value that
  waited froze the rest; rebuilt from the values applied, it carries only
  what was applied.
- **A safe state is not a stopped bridge.** Stopping hands over to ready a
  step before the model goes idle and the PWM stops, so an admission that
  read only the state could rebuild the machine while the bridge still
  switched. The inverter's admission reads the bridge as well.
- **A drive that stands may still turn.** The PWM update and the hall
  capture read the hall sensor's sector maps, and a rotor coasting below
  the runout speed keeps both firing; rebuilding the maps masks both.

- **Once configs come from the values applied, a group says nothing the
  configs do not.** What a change rebuilds is what differs between the
  configs built before and after it, and what it needs to take effect
  follows from the same comparison; a group only restated it, and a
  restatement nothing checks drifts. Comparing configs needs them to
  compare, which a defaulted `operator==` gives each.
- **A fallback needs the scope of the check, not of the config.** The
  inputs of a rule are the least that mends it and the most it may reset;
  a config holds parameters that a failed check has nothing to say about,
  the phase order among them.

## 11. Open questions

- ~~**Adapter or driver?**~~ Settled at step 9: the driver models the
  concept directly. It needed the trait constants, a default timeout and
  `erase`, which on ferroelectric memory is an overwrite with
  `erased_value` — no adapter, no second layer to keep in step.
- ~~**`settings::value` variant vs raw bytes + type tag**~~ at the
  transport boundary. Settled by phase 2, which built the variant: the OD
  accessors convert `od_value` to `settings::value` on the way in and visit
  it on the way out. Raw bytes would feed `from_sdo_data(bytes, type)`
  directly and avoid that variant-to-variant conversion, which nothing has
  asked for since.
- ~~**Exposing stored vs active value**~~ for `on_restart` parameters over
  the OD, beyond the `restart_required` and `unapplied` flags (`3000h`,
  sub-indices `11h` and `12h`). Settled on 2026-10-16 by the records (§7):
  02h of a record is the value applied, the active one, and 03h the value
  stored.
- **Counters** (hour meter, energy, fault counts) need their own append-log
  region; out of scope here, but the region layout should leave room.
- ~~**`survey()` trusts headers.**~~ Settled on 2026-10-03 by the rewrite
  of the store (§10): there is no survey and no position. Every save reads
  every slot whole and writes after the newest whole record, a header with
  nothing whole behind it counts for nothing, and a save after a load the
  medium refused writes only once every slot reads. The gap was wider than
  stated: on flash it needed no rot, since the header of a save torn in
  the last slot of a block put the position at the start of the next
  block, the newest record's. The `record_whole()` it called for is
  `check_record`, shared by the pass and `decode_record`, at the cost it
  named — a CRC per record — paid on every call rather than on that path.
