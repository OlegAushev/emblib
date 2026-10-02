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
became `placement`. The store's
algorithms, invariants and failure scenarios are described in detail, in
Russian, in `settings-store-algorithms.ru.md`.

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

auto descriptors() -> std::span<descriptor const>;
auto find(std::string_view name) -> std::optional<index>;
auto get_at(index) -> std::expected<value, error>;
auto set_at(index, value) -> std::expected<std::optional<change>, error>;
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
  0x08     4    seq        monotonic write counter
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
previous intact. Where erasing is required, slots are grouped into blocks
the size of the medium's erase unit, and only the first slot of a block
pays for an erase — by which time the newest record lives in another block.
That is asserted, not assumed: with `needs_erase`, the slots must span at
least two blocks.

Each section declares `slot_capacity` as a **constant, not derived from the
current parameter count** — otherwise adding a parameter would shift the
slot stride and invalidate everything already stored.
`static_assert(record_size <= slot_capacity)`.

For the 58 parameters of the current product: record = 488 B,
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

1. Build the record in a RAM buffer — one burst instead of 58 small writes,
   and mandatory anyway for flash granularity.
2. Pick the next slot; erase it if the medium needs it. Where it does,
   a slot inside a block is read first, and one holding debris sends the
   save to the next block (see "Every save asks which slot it takes" in
   §10).
3. Write header and cells.
4. **Write the footer last.** A record interrupted by power loss has no
   valid CRC and is invisible to the loader; the previous record is
   untouched.
5. Read back and verify the CRC. This catches a dead FRAM or a failed
   program — the stack it replaced had no such check. The in-RAM position
   does not wait for it: the slot and the sequence number are spent before
   the first write, whatever the outcome (see §10).

### Load

1. Read the 16-byte header of every slot, once per load.
2. Candidates: valid magic, known format, sane count, record fits the slot.
3. Try candidates from the highest `seq` down: read fully, check CRC; the
   first whole record wins. None valid → all defaults + a reported fault.
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
concept some_block_storage = requires {
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
  wipe honest on FRAM. `needs_erase` separately says whether `write`
  *requires* an erased target.
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
  test/mock/block_storage.hpp  [done] constexpr RAM backend for tests
  test/mock/plain_word.hpp     [done] a std::atomic stand-in for pending
                                      changes in constant expressions
  test/*_test.cpp                     in-tree convention: anonymous namespace,
                                      static_assert only

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
   `some_block_storage` directly, no adapter; it has since moved to embdev
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
  flash to save microseconds on an operation that runs twice a boot.
- **Every save asks which slot it takes.** On a medium that must be
  erased, a save reads the slot the position names before writing to it,
  unless the slot starts a block — entering a block erases it. A slot that
  is neither a record nor erased holds the debris of a save that never
  committed, and the save steps to the next block: the block it is standing
  in cannot be erased, since the record just restored may live there. The
  first version kept a flag instead, raised by a restart or a failed save
  and cleared by the first look that found the slot erased, which assumed
  the debris sits right behind the newest record. It need not: a save the
  medium refuses before its first byte still spends its slot, so the debris
  of the next attempt lies a slot further on. After a restart the look
  found that hole erased, and the save after it wrote into the debris —
  which on NOR fails the read-back with no cause, the signature reserved
  for a memory that no longer holds data. Whether a slot is erased is a
  fact about the medium, and caching it meant proving that every path to a
  position marks it right; that proof was wrong once. The look costs a
  slot read per save, a kilobyte against the milliseconds of programming,
  and compiles away where the medium needs no erase. Erasing on every
  restart instead would cost a block per boot, which on a section of 128
  slots to a block is 128 times the wear.
- **A restart takes its sequence number from the newest header and its
  position from the newest whole record.** Where the medium stopped and
  what it holds are different questions. A save interrupted before it
  committed leaves a header claiming a generation that no record backs,
  and numbering the next record below it would put two records on one
  generation, which a load can then only order by slot — so the number
  follows the header. The position cannot: on two slots the slot past the
  debris is the record just restored, and on flash a lap-old header whose
  sequence number rotted upwards — NOR loses programmed bits, and a
  sequence number is mostly zeros — would put the position in the old
  block, from where the step over the debris lands in the block of the
  record restored and erases it. Following the record instead costs, after
  a torn save whose header landed, the rest of that block: one erase per
  such incident, not per boot.
- **A load reads every header once.** One pass maps the sequence number of
  every candidate, and trying the next one reads only its record.
  Rereading the headers per candidate cost a pass for every header above
  the newest whole record — one per torn save, until the ring erases its
  block — and a pass is 256 slots in the product's flash geometry; on an
  external SPI NOR it would be half a second of loading. The map costs four
  bytes a slot of stack while `load()` or `survey()` runs. Of what a
  candidate can turn out to be, only a read the medium refused is carried
  out of the load: debris is ordinary, and outliving it is what the search
  is for.
- **The store's buffer is a slot, not a record.** A firmware that declared
  more parameters wrote a longer record, and refusing to read it would
  silently discard the settings of anyone downgrading. The RAM cost is
  `slot_capacity`, which is the application's own number — the same one it
  reserved on the medium.
- **A failed save says which step failed** — erase, body, commit or
  verify — and carries the medium's own error code, except after a
  successful write that did not read back, where there is no code to carry
  and the absence is the diagnosis.
- **The slot and the sequence number are both spent before the first
  write**, whether or not the attempt succeeds — with one exception. The
  slot, so a retry never lands on the debris of the attempt before it. The
  sequence number, because a failed save can still have landed — the record
  wrote and only the read-back failed — and reusing the number would leave
  two records claiming one generation, which a load orders by slot rather
  than by age, silently preferring the older one. The exception is an erase
  that was refused: nothing was written, so there is no debris to avoid,
  and the block still has to be erased. Spending the slot would put the
  position inside a block that was never cleared, and the step over the
  debris would then take the next block — the one holding the newest
  record. So the slot stays, and a sector that never erases makes every
  save fail with `save_stage::erase` rather than quietly turning the store
  into a single block that erases its own newest record on every lap.
- **A save before the first load surveys the headers.** Otherwise it would
  start counting from one and write a record that looks older than what is
  stored — invisible to the next load, which takes the highest sequence
  number. The regression test fails without the survey. Headers are all it
  reads, which leaves one gap — see the open question on it.
- **Tests follow the in-tree convention** (`emb/test/*_test.cpp`, anonymous
  namespace, `static_assert` only): they cost compile time and contribute no
  symbols to the image.
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

## 10a. What the switch-over changed for an operator

Two behaviours changed on purpose, and both are visible from a CANopen
tool:

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
- **`survey()` trusts headers.** A save before the first load, after a
  save that tore, is positioned by the newest header — which on two slots
  is the slot past the debris, i.e. the last good record; if that save
  tears too, nothing is left. On flash a lap-old header that rotted newer
  puts the position in the old block the same way, from where the step
  over the debris erases the block of the newest record. The application
  loads before anything can save, so the gap is latent. Closing it means
  the survey checking records as the load does: a `record_whole()` in
  `record.hpp`, shared by both, at the cost of a CRC per candidate on that
  path.
