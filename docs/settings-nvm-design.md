# Settings and NVM: design and migration plan

Status: in progress. Steps 1-3 of the plan are implemented; everything
else is designed but not written. Nothing in this document is wired into the
running firmware yet — the existing `emb::nvm::registry` stack keeps working
untouched until the switch-over phase.

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
    emb::settings     schema, descriptors, RAM image, typed access, groups
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
  bool           (*validate)(raw_value);
  bool             writable;  // may ever change at run time
  group_id         group;     // which config struct it feeds
  apply_policy     apply;     // live / on_safe_state / on_restart
  bool             expose;    // meant for external access at all
};
```

No offset and no size: every value occupies one four-byte cell, so a
parameter's index *is* its position in the RAM image, and the image is a
plain `std::array<raw_value, N>`. A bool costs three bytes of padding and
buys an image with no layout arithmetic and a record whose cells need no
per-entry size.

`writable` is not `od_access`: "immutable after production" (factory
calibration) is a fact about the parameter, while "read-only over SDO" is a
fact about the protocol and may be strictly narrower. The adapter may
narrow, never widen; checked with `static_assert`.

Type-erased interface offered to transports:

```cpp
namespace settings {
using value = std::variant<bool, std::int32_t, std::uint32_t, float>;
enum class error { unknown_name, read_only, out_of_range, type_mismatch, storage };

auto descriptors() -> std::span<descriptor const>;
auto find(std::string_view name) -> std::optional<index>;
auto get(index) -> value;
auto set(index, value) -> std::expected<void, error>;
}
```

## 4. Applying changes without a restart

Parameters split into three classes, declared in the schema:

1. `live` — feed derived state only (PI gains, limits, slopes, angle
   correction, protection thresholds). Applied by recomputation.
2. `on_safe_state` — applicable, but not in every drive state (motor
   electrical parameters that rebuild the observer, PWM frequency). The FSM
   decides; until then the change stays pending.
3. `on_restart` — change the *set* of objects that exist or the peripheral
   setup (`hall.enabled`, `motor.p`, pin routing). Not applied live; the
   system reports `restart_required` instead of pretending.

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
if (settings::changed(settings::group::model)) {
  if (!model_.configure(settings::read_model_config(), settings::read_mras_config()))
    trouble::set(trouble::invalid_config{});
  settings::acknowledge(settings::group::model);
}
```

Pull, not registration: no lifetimes, no reverse dependency, no hidden
observers, and the safe point sits literally in the code that knows it is
safe. The cost is one atomic load per control cycle. `md::motor_drive`
already hand-rolls this pattern in `pending_pwm_freq_` /
`pending_calibration_`; the mechanism generalizes that single instance.

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

## 5. NVM record format

One self-describing record per section, not a fixed-address cell array:

```
record (16 + 8N bytes, 4-aligned):
  offset  size  field
  0x00     4    magic     section marker
  0x04     2    format    layout version of the record itself (not the schema)
  0x06     2    count     number of cells
  0x08     4    seq       monotonic write counter
  0x0C  8*N     cells     N x { u32 id; u32 raw }
  ...      4    crc32     over everything above; written last = commit
```

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

Each section declares `slot_capacity` as a **constant, not derived from the
current parameter count** — otherwise adding a parameter would shift the
slot stride and invalidate everything already stored.
`static_assert(record_size <= slot_capacity)`.

For the 57 parameters of the current product: record = 472 B,
`slot_capacity` = 1024 B (room for about 125).

- FRAM (FM25W256, 32 KB): two slots, 2 KB of 32; endurance is a non-issue.
- EEPROM: identical; page splitting is the driver's business.
- Internal flash (APM32F405/F407, 16 KB sectors at the bottom of the
  bank): two sectors, records appended at `slot_capacity` stride — 16
  slots per sector; on overflow switch to the other sector (erased
  before its first write), erase the old one lazily at the next
  rollover. 32 saves per erase cycle; at 10k cycles, hundreds of
  thousands of saves. Atomicity and wear levelling from one mechanism.

### Commit protocol (identical on all three media)

1. Build the record in a RAM buffer — one burst instead of 57 small writes,
   and mandatory anyway for flash granularity.
2. Pick the next slot; erase it if the medium needs it.
3. Write header and cells.
4. **Write `crc32` last.** A record interrupted by power loss has no valid
   CRC and is invisible to the loader; the previous record is untouched.
5. Read back and verify the CRC; only then update the in-RAM notion of the
   active slot. This catches a dead FRAM or a failed program — the current
   stack has no such check.

### Load

1. Read the 12-byte header of every slot.
2. Candidates: valid magic, known format, sane count, record fits the slot.
3. Read candidates fully, check CRC, take the highest `seq`. None valid →
   all defaults + a reported fault.
4. Per cell: binary search `id` in the constexpr descriptor table sorted by
   id. Unknown id → ignored (removed parameter). Found → **validated against
   min/max/predicate before entering the image**; out of range → default,
   counted. Corrupt storage must not inject a bad value into the loops.
5. Parameters absent from the record keep their defaults (new in this
   firmware).
6. Produce a load report (`slot`, `seq`, `loaded`, `defaulted_missing`,
   `defaulted_invalid`) and expose it for diagnostics. Today "the parameter
   did not read" is indistinguishable from "the parameter is like that".

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

### Rejected alternatives

- **Fixed-offset cells (the current scheme).** Same overhead (8 B per
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
  (`emb::nvm::error` for the FRAM driver, an HAL status for flash); a common
  enum here would only add a mapping layer at the wrong end.
- Erase geometry is deliberately absent: F4 sectors are non-uniform
  (16K/64K/128K in one bank), so no constant describes them. Alignment is a
  property of the declared layout; the backend rejects a range that does not
  cover whole blocks.

## 7. CANopen binding

The OD table keeps what genuinely belongs to CANopen — index/subindex,
display name, categories, unit, access — and pulls type, default and
validation from the schema by name:

```cpp
inline constexpr auto config_section = od::settings_section({
  {{0x3002, 0x01}, "drive.phase_swap",   "config", "drive", "",     od_access::rw},
  {{0x3002, 0x02}, "model.torque_slope", "config", "drive", "pu/s", od_access::rw},
});
```

The name is written once instead of three times, `od_value_type` is derived
from the parameter type (they can no longer disagree), and the accessors
become two shared non-template functions plus a two-instruction thunk per
row instead of 2N instantiated bodies.

The `od_key <-> name` mapping stays hand-maintained on purpose: OD indices
are an external contract frozen for tools and EDS. Moving them into the
schema would not reduce maintenance, only put a protocol fact in the wrong
file.

Completeness is still checked at compile time, without coupling:

- no duplicate `od_key` and no duplicate name within a section;
- every schema parameter with `expose` appears exactly once — "forgot to
  publish the new parameter" fails the build;
- `od_access` no wider than `writable`;
- `od_value_type` compatible with the parameter type, including
  `named_unit` types through `.value()`.

## 8. File layout

```
external/emblib/emb/
  meta/fixed_string.hpp        [done] structural string for NTTP names
  nvm/storage.hpp              [done] block storage concept + is_erased
  settings/value.hpp           [done] value_type, value, cell conversions
  settings/param.hpp                  param<Name, T>: def/min/max/validate,
                                      writable, group, apply_policy, expose
  settings/schema.hpp                 make_schema, consteval lookup by name,
                                      descriptor table, static_asserts
  settings/image.hpp                  RAM image, typed get/set, dirty groups
  settings/record.hpp                 record layout, build and parse
  settings/store.hpp                  slots, active record, commit, load report
  can/canopen/od_settings.hpp         OD section generated from the schema
  test/mock/block_storage.hpp  [done] constexpr RAM backend for tests
  test/*_test.cpp                     in-tree convention: anonymous namespace,
                                      static_assert only

src/common/nvm/
  fram_block_storage.hpp              fm25w256 adapter to the new concept
                                      (or extend the driver itself, see 10)

src/app/inverter/settings/
  schema.hpp                          the product's parameter list
  params.hpp / params.cpp             load/store/get/set/changed/acknowledge,
                                      section-to-slot layout, hw::nvm binding
```

## 9. Implementation plan

Each step is a separate commit; every step through phase 1 leaves the
firmware behaviourally unchanged.

**Phase 0 — foundation in emblib, nothing existing is touched.**

1. `meta/fixed_string.hpp` — **done**
2. `nvm/storage.hpp` + mock backend — **done**
3. `settings/value.hpp` — **done**
4. `settings/param.hpp` + `settings/schema.hpp`
5. `settings/image.hpp`
6. `settings/record.hpp`
7. `settings/store.hpp`
8. Store tests: two mock media (FRAM-like; flash-like with granularity,
   write-once and block erase) and the scenarios — clean memory, power cut
   at every step of the commit, added/removed parameter, retyped parameter,
   out-of-range value, sector overflow and rollover.

**Phase 1 — the application, alongside the old stack, nothing switched.**

9. `src/common/nvm/fram_block_storage.hpp`
10. `src/app/inverter/settings/schema.hpp` (same parameters, plus ranges,
    groups, apply policy)
11. `src/app/inverter/settings/params.hpp/.cpp`

Not called by anything yet; instantiation is verified with `nm` on the
object file, not the map — `--gc-sections` drops unreferenced code.

**Phase 2 — consumers, one at a time.**

12. `read_*_config()` move to the new image **keeping their signatures**;
    `main.cpp` changes by one line (`settings::init` -> `settings::load`).
13. OD `config` section generated from the schema, using per-row thunks so
    emblib's `od.hpp` needs no change; the tagged accessor in `od_object` is
    a later, optional optimization.
14. `save_all_parameters` / `restore_*` / `erase_*` / `save_hall_calibration`
    move to `store()`; hall calibration becomes atomic.
15. `configure()` entry points and dirty groups.
16. Cleanup: delete `parameters.hpp`, the old `settings.hpp/.cpp` parts,
    `od_nvm.hpp`, move `emb/nvm.hpp` to obsolete.

Verification after each step of phases 1-2: both presets
(`miniboard-debug`, `rev-a-debug`); for emblib headers additionally a host
`g++ -std=c++26 -Wall -Wextra -Wconversion` run.

Stored data is not migrated: the format is new and the first boot after
phase 2 comes up with defaults. Decided deliberately — no converter.

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
- **Wrapper types are recognised structurally**, by `value_type` + explicit
  constructor + `value()`, not by naming `units::named_unit` and
  `emb::clamped`. `settings/value.hpp` therefore depends on neither header,
  and `emb::clamped` parameters come for free — which lets the application
  schema declare `unsigned_pu_f32` directly and drop the wrapping that
  `read_*_config()` does by hand today.
- **`from_raw` is total, `from_value` is not.** A cell read back from
  storage may hold any bit pattern and must yield a value rather than a
  trap: a bool is any-non-zero rather than a `bit_cast`, and a float may
  come back NaN — which the range check then rejects, since NaN compares
  false against both bounds. A protocol write carrying the wrong type, by
  contrast, is rejected outright.
- **Cells are ordered under their type tag**, never as raw words: as int32
  the cell `0xFFFFFFFB` is -5 and belongs in [-10, 10], while as a word it
  is above every positive bound.
- **Tests follow the in-tree convention** (`emb/test/*_test.cpp`, anonymous
  namespace, `static_assert` only): they cost compile time and contribute no
  symbols to the image.

## 11. Open questions

- **Adapter or driver?** With the concept spelled `write`, `fm25w256::fram`
  is nearly a model already: it needs a default `timeout`, the trait
  constants and `erase`. Extending the driver in `src/common/` may be
  cheaper than a separate adapter. Decide at step 9.
- **`settings::value` variant vs raw bytes + type tag** at the transport
  boundary. The variant is friendlier to future consumers; raw bytes would
  feed `make_od_value(raw, type)` directly and avoid a variant-to-variant
  conversion.
- **Exposing stored vs active value** for `on_restart` parameters over the
  OD, beyond a `restart_required` flag and a `pending_changes` mask.
- **Counters** (hour meter, energy, fault counts) need their own append-log
  region; out of scope here, but the region layout should leave room.
