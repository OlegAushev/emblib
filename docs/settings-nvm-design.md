# Settings and NVM: design and migration plan

Status: done. Every step of the plan is implemented: the settings live in
a schema with bounds, groups and apply policies; they are stored as whole
records that commit or do not; they are reached by name from the
application and by index from a protocol; and a change takes effect without
a restart wherever the schema says it may. The stack it replaced is gone.

Two parts of the design never became steps and are not built: the trigger
that applies a batch whole and the debounced save (§4, §10a), and the
sections beyond `config` (§5). Checked against the code on 2026-09-28. The
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
`settings-store-algorithms.ru.md`.

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
    emb::settings     schema, descriptors, RAM image, typed access, groups,
                      section
        ^                          ^
        |                          |
    app schema                 emb::can::canopen::od_settings
    (parameter list)           (adapter: OD table -> schema by name)
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
  std::uint32_t    id;        // hash of name mixed with the type code
  value_type       type;
  raw_value        def, min, max;  // cells, ordered under `type`
  group_id         group;     // which config struct it feeds
  apply_policy     apply;     // live / on_safe_state / on_restart
  bool             writable;  // may ever change at run time
  bool             expose;    // meant for external access at all
};
```

Declared as:

```cpp
inline constexpr auto schema = settings::make_schema(
  param("drive.phase_swap", false, {.group = group::drive}),
  param("drive.runout_speed", rpm{100.0f},
        {.min = rpm{0.0f}, .max = rpm{5000.0f},
         .group = group::drive, .apply = apply_policy::live}),
  param("drive.stopping_torque", pu{0.05f},          // clamped: own bounds
        {.group = group::drive, .apply = apply_policy::live}));
```

The type comes from the default, so a literal needs its suffix — `param("x",
0.05)` does not compile. Per-parameter checks (default within bounds, min
below max) live in `param()` so that a bad declaration is reported on its
own line; cross-parameter ones (duplicate name, identifier collision) live
in `make_schema`.

No offset and no size: every value occupies one four-byte cell, so a
parameter's index *is* its position in the RAM image, and the image is a
plain `std::array<raw_value, N>`. A bool costs three bytes of padding and
buys an image with no layout arithmetic and a record whose cells need no
per-entry size.

`writable` is not `od_access`: "immutable after production" (factory
calibration) is a fact about the parameter, while "read-only over SDO" is a
fact about the protocol and may be strictly narrower. The adapter may
narrow, never widen: binding a parameter that is not `writable` with `rw`
fails the build at the row (§7).

Type-erased interface offered to transports:

```cpp
namespace settings {
using value = std::variant<bool, std::int32_t, std::uint32_t, float>;
enum class error { unknown_parameter, read_only, type_mismatch, out_of_range };

std::span<descriptor const> descriptors();
std::optional<index> find(std::string_view name);
std::expected<value, error> get_at(index);
std::expected<std::optional<change>, error> set_at(index, value);
}
```

## 4. Applying changes without a restart

Parameters split into three classes, declared in the schema:

1. `live` — feed derived state only (PI gains, limits, slopes, angle
   correction, protection thresholds). Applied by recomputation. The PWM
   frequency is live as well: the ADC interrupt switches the timer and the
   timesteps between two control steps, and the preloaded period and
   duties start together at the next update event.
2. `on_safe_state` — applicable, but not in every drive state (motor
   electrical parameters that rebuild the observer, the hall sensor's
   sector angles). The FSM decides; until then the change stays pending.
3. `on_restart` — change the *set* of objects that exist or the peripheral
   setup (`hall.enabled`, pin routing), or are handed to an object only
   when it is built (`motor.p`, `isense.zero_drift_th`). Not applied live;
   the system reports `restart_required` instead of pretending.

Mechanism: **the owner pulls at a safe point**, nothing is pushed from the
communication task.

- Every object that derives state from a config gains
  `configure(config const&)`; its constructor delegates to it. Construction
  splits into *acquiring dependencies* (references, peripherals — immutable
  for life) and *parametrization* (recomputable).
- The settings layer keeps an atomic dirty mask over groups. An SDO write
  validates, updates the RAM image and sets a bit. Nothing blocking or
  recomputing happens in the CAN context.
- The owner checks the mask where it knows the state is consistent:

```cpp
if (settings::config.pending().take(group::model, apply_policy::live)) {
  if (!model_.configure(settings::read_model_config(),
                        settings::read_mras_config()))
    trouble::set(trouble::invalid_config{});
}
```

One operation tests and clears, rather than a query followed by an
acknowledgement: a change that lands between the two would otherwise be
dropped — its bit cleared, its value never applied. Taking first and
reconfiguring afterwards can at worst apply the same value twice.

`up_to` is what the caller can honour: a running drive takes
`apply_policy::live` and leaves `on_safe_state` changes waiting for a state
where they are safe; nothing takes `on_restart`, which is what keeps
`restart_required` standing until the restart.

Pull, not registration: no lifetimes, no reverse dependency, no hidden
observers, and the safe point sits literally in the code that knows it is
safe. The cost is one atomic load per control cycle. `md::motor_drive`
hand-rolled this pattern in `pending_pwm_freq_` / `pending_calibration_`;
the mechanism generalizes that single instance.

`configure()` decides explicitly what survives a reconfiguration: changing
`Kp/Ki` keeps the integrator (they are tuned while running, and a reset
would jerk the torque), changing motor parameters resets the MRAS observer.
Blind reconstruction has no vocabulary for that distinction, which is why
`configure()` beats "build a new object".

Anti-pattern, deliberately rejected: handing deep objects a reference into
the settings image to read parameters lazily. It makes `pmsm/` and `sense/`
depend on `settings/`, lets a value change between two reads inside one
control step, and throws away precomputation.

Batches (the twelve hall calibration angles) must not be applied halfway:
application is triggered by an explicit command (`1010h` save+apply) or by a
quiet-period debounce, and group validation runs before the swap, keeping
the old config on failure.

Not built. A calibration result reaches the image in one call, which
nothing can split; twelve writes over SDO are twelve changes, and standing
still, each is applied at the next task tick, so the sensor is rebuilt from
a mix of old and new angles until the last one lands — harmless, since no
current flows. The validation has nothing to check with yet:
`emb::hall::validate(calibration_result)` accepts any angles.

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
- `id = fnv1a32_continue(fnv1a32(name), type_code)`. Retyping a parameter
  under the same name would otherwise pass unnoticed — same four bytes, same
  id, garbage in the control loop. Mixing the type in makes the old cell
  simply not match, and the parameter comes up with its default. Uniqueness
  of `id` within a section is a compile-time assert.
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

The OD table keeps what genuinely belongs to CANopen — index/subindex,
display name, categories, unit — and pulls access and validation from the
schema by name. The type is pulled from the schema as well, and written in
the row besides, so that a text parser can build a host's table from the
source; the dictionary checks that the two agree. Built on 2026-09-29 as a
binding per row of the application's constexpr table
(`can/canopen/od_settings.hpp`, bound to a section by `od_settings_for` in
`can/canopen/od_section.hpp`; the dictionary around it is described in
`canopen-od-plan.ru.md`):

```cpp
using param = emb::can::canopen::od_settings_for<settings::config>;

inline constexpr emb::can::canopen::od_row<context> rows[] = {
  {{0x3002, 0x01}, "config", "drive", "phase_swap",   "",     boolean, param::rw<"drive.phase_swap">},
  {{0x3002, 0x02}, "config", "drive", "torque_slope", "pu/s", float32, param::rw<"model.torque_slope">},
  // ...
};
```

The name is written once instead of three times, and the type written in
the row must be the parameter's, so the two cannot disagree. A row carries
the parameter's index as its argument, and one shared function reads, one
writes and one restores every parameter: the 116 per-parameter thunks are
gone.

The `od_key <-> name` mapping stays hand-maintained on purpose: OD indices
are an external contract frozen for tools and EDS. Moving them into the
schema would not reduce maintenance, only put a protocol fact in the wrong
file.

Completeness is checked at compile time, without coupling:

- no duplicate `od_key` and no duplicate name in the dictionary, for every
  row, settings or not;
- every schema parameter with `expose` has exactly one row and a hidden one
  has none — "forgot to publish the new parameter" fails the build. Every
  row of the bridge carries the schema's list of names, so the check runs as
  soon as the dictionary binds one parameter;
- `rw` on a parameter that is not `writable` fails at the row; `ro` may
  narrow;
- the type written in the row must equal the parameter's, `named_unit` and
  `emb::clamped` included (T15 in `canopen-od-algorithm.ru.md`).

Restoring one default over 1011h:04 goes through the by-index restore, which
refuses what the protocol may not write, the same rule as for restoring
all defaults. The dictionary no longer holds defaults.

## 8. File layout

```
external/emblib/emb/
  meta/fixed_string.hpp        [done] structural string for NTTP names
  nvm/storage.hpp              [done] block storage concept + is_erased
  settings/value.hpp           [done] value_type, value, cell conversions
  settings/param.hpp           [done] param(): def/min/max, writable, group,
                                      apply_policy, expose
  settings/schema.hpp          [done] make_schema, lookup by name and by id,
                                      descriptor table, uniqueness checks
  settings/image.hpp           [done] RAM image, typed and erased access
  settings/pending.hpp         [done] dirty groups, split by apply policy
  settings/record.hpp          [done] record layout, encode and decode
  settings/store.hpp           [done] slots, active record, commit, load
                                      report
  settings/section.hpp         [done] one section: the image, its record,
                                      pending changes; load/save/wipe
  can/canopen/od_settings.hpp  [done] per-row bindings of the schema's
                                      parameters, see §7
  can/canopen/od_section.hpp   [done] od_settings_for a section, and the
                                      readers of its state under 3000h
  test/mock/ram_storage.hpp  [done] constexpr RAM backend for tests
  test/mock/plain_word.hpp     [done] a std::atomic stand-in for pending
                                      changes in constant expressions
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
                                      bounds, groups and apply policies
  params.hpp / params.cpp      [done] the config section; load/save/wipe
                                      and the service warnings as trouble
  settings.hpp / settings.cpp  [done] the config readers and the saving of
                                      a hall calibration
```

## 9. Implementation plan

Each step is a separate commit; every step through phase 1 leaves the
firmware behaviourally unchanged.

**Phase 0 — foundation in emblib, nothing existing is touched.**

1. `meta/fixed_string.hpp` — **done**
2. `nvm/storage.hpp` + mock backend — **done**
3. `settings/value.hpp` — **done**
4. `settings/param.hpp` + `settings/schema.hpp` — **done**
5. `settings/image.hpp` + `settings/pending.hpp` — **done**
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
15. `configure()` entry points and dirty groups — **done**. The seam is
    the drive's periodic task tick, which already runs in task context with
    the control timebase masked: `motor_drive::apply_pending_settings()`
    decides from its own state what it can promise (`on_safe_state` when
    standing still, `live` otherwise) and applies the groups it can reach —
    drive, motor, model and hall, and pwm since the PWM frequency is
    stored. `pmsm::model`, `pmsm::mras_observer` and `hall::angle_sensor`
    grew a `configure()`; each keeps what it has learned, since gains are
    tuned while running. Latency is one task tick, 33 ms.
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
  `unknown parameter 'motor.p'` in the diagnostic line itself.
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
- **`from_raw` is total, `from_value` is not.** A cell read back from
  storage may hold any bit pattern and must yield a value rather than a
  trap: a bool is any-non-zero rather than a `bit_cast`, and a float may
  come back NaN — which the range check then rejects, since NaN compares
  false against both bounds. A protocol write carrying the wrong type, by
  contrast, is rejected outright.
- **Cells are ordered under their type tag**, never as raw words: as int32
  the cell `0xFFFFFFFB` is -5 and belongs in [-10, 10], while as a word it
  is above every positive bound.
- **No per-parameter `validate` hook.** Bounds cover what a single value can
  be judged on; anything else is a relation between parameters, and that
  check belongs to the owner's `configure()`, which sees the whole config
  struct. A type-erased hook would also need a thunk per parameter for no
  gain.
- **`group_id` takes any scoped enum with a one-byte underlying type.** The
  set of groups belongs to the product, and the runtime table can only hold
  a number; requiring `: std::uint8_t` rather than truncating turns an enum
  that does not fit into a compile error.
- **`apply_policy` defaults to `on_restart`.** Opting into live application
  is a claim about the consuming code; a parameter whose policy was never
  considered should cost a restart rather than silently take effect halfway
  through a control cycle.
- **A bool cell holds 0 or 1.** `less_equal` compares bool cells as words,
  so a cell holding anything else is out of range and falls back to the
  default — even though `from_raw` would read it as true. Totality and
  range checking are separate mechanisms and both are wanted.
- **The image holds no atomics.** It is plain data with typed and erased
  access, which keeps it usable in constant expressions — and therefore
  testable the way everything else in the library is. Sharing one between
  contexts is the application's business; the image only reports what a
  write changed, and `pending_changes` records it.
- **`take()` instead of `changed()` + `acknowledge()`.** Test-and-clear in
  one operation cannot drop a change that arrives between the two calls.
- **A write that leaves a cell as it was reports no change**
  (`std::nullopt`). Otherwise a profile written whole, or a restore of
  defaults that were already there, marks every group it touches: live
  tuning of a group freezes behind a restart nothing needs, and
  `restart_required()` stands for nothing. A value changed and changed back
  stays marked: the masks record changes, not a difference from what was
  applied.
- **`pending_changes` is templated on its word type**, so the bit
  arithmetic is checked in constant expressions with a plain word while
  production uses `std::atomic<std::uint32_t>`; the test also instantiates
  the atomic form so it is compiled for the target.
- **The erased accessors are spelled apart from the typed ones** —
  `get_at`/`set_at` by index, `get`/`set` by name, `cell`/`assign_cell` for
  raw cells. Not merely different addressing: the by-index path enforces
  `writable` and the by-name path does not, and one name for two promises
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
  row names its parameter with `od_settings<...>::rw<"name">` inside the
  ordinary table, so settings objects and the others share one table, one
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

## 10b. What applying taught us

- **A group is applied whole, so a group that also waits on something
  stricter must not be applied at all.** Group `drive` holds fifteen live
  protection thresholds next to `drive.phase_swap`, which may only change
  standing still; taking the live part would rebuild `drive_config` from
  values that include the swap. The rule lives inside `take()` rather than
  in its callers: a caller that forgets it gets no diagnostic, only a phase
  swap in a spinning machine. A consequence worth knowing: one parameter
  needing a restart freezes live tuning of its group until the restart,
  which is the safe direction.
- **What `take()` cannot promise**: a stricter change marked after it
  returned is in the image by the time the caller reads its configuration.
  Narrowing that window inside the masks does not close it — the take and
  the reading of the configuration would have to be one operation. Where it
  matters, keep marking and applying out of each other's way; in the
  inverter both run as tasks in the same loop.
- **Reconfiguration is not atomic against the current loop.** The task
  tick masks the timebase and below; the ADC and PWM interrupts (priority
  0 and 2) still preempt. No value tears — every one is a word — and each
  group's derived state tolerates a single control step built from a mix of
  old and new coefficients. The one structure that would not, the angle
  sensor's sector maps, is only ever rebuilt in a state where no current
  flows, which is what its `on_safe_state` policy is for.
- **`isense.zero_drift_th` went back to `on_restart`.** Nothing can reach
  the zero-drift calibrator to tell it otherwise: it is not owned by the
  drive and has no periodic task of its own. Declaring it live would have
  been a promise the code does not keep.

## 11. Open questions

- ~~**Adapter or driver?**~~ Settled at step 9: the driver models the
  concept directly. It needed the trait constants, a default timeout and
  `erase`, which on ferroelectric memory is an overwrite with
  `erased_value` — no adapter, no second layer to keep in step.
- ~~**`settings::value` variant vs raw bytes + type tag**~~ at the
  transport boundary. Settled by phase 2, which built the variant: the OD
  accessors convert `od_value` to `settings::value` on the way in and visit
  it on the way out. Raw bytes would feed `make_od_value(raw, type)`
  directly and avoid that variant-to-variant conversion, which nothing has
  asked for since.
- **Exposing stored vs active value** for `on_restart` parameters over the
  OD, beyond the `restart_required` and `changes_pending` flags
  (`3000h`, sub-indices `11h` and `12h`).
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
