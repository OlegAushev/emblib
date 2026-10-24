#pragma once

#include <emb/assert.hpp>
#include <emb/meta/typelist.hpp>
#include <emb/meta/unroll.hpp>
#include <emb/nvm/storage.hpp>
#include <emb/settings/image.hpp>
#include <emb/settings/param.hpp>
#include <emb/settings/rule.hpp>
#include <emb/settings/schema.hpp>
#include <emb/settings/store.hpp>

#include <algorithm>
#include <array>
#include <bitset>
#include <concepts>
#include <expected>
#include <functional>
#include <optional>
#include <utility>
#include <variant>

#include <cstddef>
#include <cstdint>

namespace emb {
namespace settings {

// The scoped enumeration `apply_refusal` defines why `section::apply` or
// `section::apply_all` applied nothing.
enum class apply_refusal : std::uint8_t {
  // Index that names no parameter, returned by `section::apply`.
  unknown_parameter,
  // A parameter whose apply policy is `apply_policy::on_restart`, returned
  // by `section::apply`: nothing applies it on command, whatever its value.
  not_applicable,
  // The checker refused the values that the change would bring about.
  invalid,
  // The applier did not admit the change in its present state.
  not_admitted,
};

// The scoped enumeration `save_refusal` defines why `section::save` wrote
// nothing, or failed to.
enum class save_refusal : std::uint8_t {
  // A parameter applied on command holds a value other than the one applied:
  // a save keeps what the machine runs on, not what waits to be applied.
  unapplied,
  // The checker refused the values that the next startup would load.
  invalid,
  // The store failed to write the record.
  medium,
};

// The class template `section_save_failure` tells why `section::save` wrote
// nothing or failed to: the `reason`, and for `save_refusal::medium` what the
// store reported, `store`.
template<typename Error>
struct section_save_failure {
  save_refusal reason;
  std::optional<save_failure<Error>> store{};
};

// A refusal of `section::apply` or `section::apply_all`, or of
// `section::save`: what `section::last_refusal` keeps of the last one.
using refusal = std::variant<apply_refusal, save_refusal>;

// The concept `some_checker<C, Schema>` is satisfied if and only if `C`
// checks values together: `checker.check(values)` returns whether the
// parameters may take their values in `values`, an image of `Schema`,
// together, which the bounds of each alone cannot tell. A check depends on
// the values alone, so its answer holds wherever it is asked: before a
// section applies the values, before it saves them, and when the application
// builds its state after a load.
template<typename C, auto& Schema>
concept some_checker = requires(C& checker, image<Schema> const& values) {
  { checker.check(values) } -> std::same_as<bool>;
};

// The concept `some_applier<A, Schema>` is satisfied if and only if `A` is a
// checker that also brings the state the application derives from the
// parameters to the values applied: `applier.lock()` returns a guard, an
// object that holds that state still for as long as it lives, whether or not
// it can be copied or moved; `applier.admit(before, after)` returns whether
// the values applied may change from `before` to `after` in the present
// state; and `applier.configure(before, after)` rebuilds what derives from
// the parameters that differ between `before`, the values applied until
// then, and `after`, the values applied now, and cannot fail: whatever may
// refuse is asked before, by `check` or `admit`.
template<typename A, auto& Schema>
concept some_applier = some_checker<A, Schema>
                    && requires(A& applier,
                                image<Schema> const& before,
                                image<Schema> const& after) {
                         applier.lock();
                         { applier.admit(before, after) } -> std::same_as<bool>;
                         applier.configure(before, after);
                       };

// The class template `section` binds the schema `Schema` to the place
// `Placement` on a medium of type `Storage`. It holds the values requested,
// an image of the parameters that it loads from the medium and saves there,
// and keeps two more images: the values the next startup would load, and the
// values applied, out of which the application builds the state it derives
// from the parameters. A write through `set`, `set_at` or
// `restore_all_defaults` changes the values requested only. `load` sets all
// three images; `apply` and `apply_all` record values requested as applied,
// once an applier has checked them, admitted them and rebuilt its state from
// them; `discard` and `discard_all` put the values applied back into the
// values requested. `unsaved`, `unapplied` and `restart_required` compare the
// images cell by cell, i.e. by encoding, so a value written and then put back
// leaves nothing to save or to apply; `-0.0f` does not put back `+0.0f`.
// `requested`, `applied` and `stored` return the three images.
//
// A checker refuses every change and every save while the values applied
// break it, since `apply` checks the values applied with the change taken in
// and `save` the values requested: an application that checks the values
// loaded calls `fall_back_to_defaults` right after `load`, before it builds
// anything from the values applied.
//
// A section can be constant-initialized and then holds the defaults in all
// three images. The behavior is undefined if `save` or `wipe` is called
// before `load` has bound the section to a medium; the other member functions
// may be called before it.
//
// The section is not synchronized: a call to a non-const member function must
// not run concurrently with any other call on the section.
template<auto& Schema, nvm::some_storage Storage, placement Placement>
class section {
  using store_type = store<Schema, Storage, Placement>;
  using error_type = typename Storage::error_type;

  static constexpr std::size_t count = image<Schema>::count;

  // Cells, by index, that a call applies.
  using selection = std::bitset<count>;

  image<Schema> requested_;
  image<Schema> stored_;
  image<Schema> applied_;
  load_result last_load_;
  std::optional<std::uint32_t> sequence_;
  std::optional<refusal> last_refusal_;
  std::optional<store_type> store_;

public:
  using storage_type = Storage;

  static constexpr auto& schema = Schema;
  static constexpr auto& placement = Placement;

  // Binds the section to `medium` and restores the values requested from the
  // newest whole record there, or from the defaults if there is none, as
  // `store::load` does, and takes what it restored as applied: the
  // application builds the state it derives from the parameters after the
  // load. Returns what the load found, which `last_load()` keeps. The section
  // keeps a reference to `medium` for `save` and `wipe` until a later call to
  // `load` replaces it.
  constexpr load_result load(Storage& medium)
  {
    store_.emplace(medium);
    last_load_ = store_->load(requested_);
    stored_ = requested_;
    applied_ = requested_;
    sequence_ = last_load_.record.valid
                  ? std::optional<std::uint32_t>{last_load_.record.seq}
                  : std::nullopt;
    return last_load_;
  }

  constexpr load_result const& last_load() const
  {
    return last_load_;
  }

  // Writes the values requested to the medium as the next record, as
  // `store::save` does, if every parameter applied on command holds its value
  // applied and `checker` accepts them: the next startup then loads what the
  // machine runs on, and the values waiting for a restart as checked. Every
  // refusal, the medium's included, is kept as `last_refusal()`. After a
  // successful write, `unsaved()` is `false` until a value changes, and
  // `sequence()` returns the number of the record written. The values applied
  // are left as they are.
  template<some_checker<Schema> C>
  constexpr std::expected<void, section_save_failure<error_type>>
  save(C& checker)
  {
    EMB_PRE(store_.has_value());
    using failure = section_save_failure<error_type>;

    if (unapplied()) return refuse(failure{save_refusal::unapplied});
    if (!checker.check(requested_)) {
      return refuse(failure{save_refusal::invalid});
    }

    auto const saved = store_->save(requested_);
    if (!saved) return refuse(failure{save_refusal::medium, saved.error()});
    stored_ = requested_;
    sequence_ = saved->seq;
    return {};
  }

  // Brings every slot of the section on the medium to the erased state, as
  // `store::wipe` does. The values requested and applied are left as they
  // are; after a successful wipe, `unsaved()` compares the values requested
  // with the defaults, which the next startup would load, and `sequence()`
  // returns nothing.
  constexpr std::expected<void, error_type> wipe()
  {
    EMB_PRE(store_.has_value());
    auto const result = store_->wipe();
    if (result) {
      stored_ = image<Schema>{};
      sequence_.reset();
    }
    return result;
  }

  // Returns the sequence number of the record that the section last restored
  // or wrote, or nothing if there is none: before the first `load`, after a
  // `load` that restored no record and after a successful `wipe`. `load` sets
  // it to the number of the record restored, or to nothing if none was; a
  // successful `save` sets it to the number of the record written; a failed
  // `save` or `wipe` leaves it unchanged. It is no number rather than zero,
  // as the numbers wrap around through zero, which numbers a record like any
  // other.
  //
  // A save numbers its record from the newest whole record on the medium,
  // not from `sequence()`: with the number of that record plus the distance,
  // in slots along the ring, from that record to the slot the save takes, or
  // with one if the medium holds no whole record. The distance exceeds one by
  // the number of slots that the save stepped over because they were not
  // erased, e.g. because they hold the debris of failed saves.
  constexpr std::optional<std::uint32_t> sequence() const
  {
    return sequence_;
  }

  // Checks whether the values requested differ from what the next startup
  // would load, which before `load` is taken to be the defaults. A failed
  // `save` or `wipe` does not change what they are compared with, even if it
  // changed the medium.
  constexpr bool unsaved() const
  {
    for (auto i = 0uz; i < count; ++i)
      if (differs(requested_, stored_, i)) return true;
    return false;
  }

  // Checks whether a parameter applied on command holds a value other than
  // the one applied.
  constexpr bool unapplied() const
  {
    return pending().any();
  }

  // Checks whether a restart would change a parameter applied only after a
  // restart: whether one holds a value other than the one applied, or the
  // next startup would load such a value. A value saved and then put back
  // still owes the restart that would load it, until it is saved again.
  constexpr bool restart_required() const
  {
    for (auto i = 0uz; i < count; ++i) {
      if (applies_on_restart(i)
          && (differs(requested_, applied_, i)
              || differs(stored_, applied_, i))) {
        return true;
      }
    }
    return false;
  }

  // -- Applying --

  // Applies the value requested of the parameter at `index`, as `apply_all`
  // applies all of them, if it differs from the one applied: the checker sees
  // the values applied with this one taken in. Refused with
  // `apply_refusal::unknown_parameter` if `index >= count`, and with
  // `apply_refusal::not_applicable` if the parameter is applied only after a
  // restart, whatever its value.
  template<some_applier<Schema> A>
  constexpr std::expected<void, apply_refusal> apply(std::size_t index,
                                                     A& applier)
  {
    if (index >= count) return refuse(apply_refusal::unknown_parameter);
    if (applies_on_restart(index)) return refuse(apply_refusal::not_applicable);
    selection chosen;
    chosen.set(index, differs(requested_, applied_, index));
    return commit(chosen, applier);
  }

  // Applies the value requested of every parameter applied on command that
  // differs from the one applied, all of them or none; a parameter applied
  // only after a restart keeps waiting for it. `applier` checks the values
  // applied with those taken in, before anything changes; then, under its
  // lock, admits the change; only then are the values recorded as applied,
  // and `applier` configures what derives from them, given the values applied
  // before and after. A refusal of the check or of the admission refuses the
  // whole and is kept as `last_refusal()`. With nothing to apply, succeeds
  // without calling `applier`.
  template<some_applier<Schema> A>
  constexpr std::expected<void, apply_refusal> apply_all(A& applier)
  {
    return commit(pending(), applier);
  }

  // Returns the last refusal of `apply`, `apply_all` or `save`, or nothing if
  // there was none. A success leaves it as it was.
  constexpr std::optional<refusal> last_refusal() const
  {
    return last_refusal_;
  }

  // -- Discarding --

  // Puts the value applied of the parameter at `index` back into the values
  // requested, whatever its apply policy. Returns `error::unknown_parameter`
  // if `index >= count`, otherwise `error::read_only` if the parameter is not
  // `writable`: what a protocol may not write, it may not put back either.
  constexpr std::expected<void, error> discard(std::size_t index)
  {
    if (index >= count) {
      return std::unexpected(error::unknown_parameter);
    }
    if (!Schema.parameters[index].writable) {
      return std::unexpected(error::read_only);
    }
    requested_.assign_cell(index, applied_.cell_at(index));
    return {};
  }

  // Puts the values applied of the `writable` parameters back into the
  // values requested; the others keep theirs.
  constexpr void discard_all()
  {
    for (auto i = 0uz; i < count; ++i) {
      if (Schema.parameters[i].writable) {
        requested_.assign_cell(i, applied_.cell_at(i));
      }
    }
  }

  // Records the defaults of the inputs of every rule of `R...` that the
  // values applied break as their values applied, pass after pass until
  // every rule holds, and leaves the values requested as they are: for an
  // application that finds the values loaded invalid and builds what derives
  // from them out of the defaults instead. The values loaded then wait, to be
  // applied once corrected, or discarded. Returns, for each rule, whether its
  // inputs fell back. A pass checks every rule before it falls back any, so
  // the order of the rules does not matter, and a rule that fell back holds
  // from then on, its inputs staying at their defaults: there is at most one
  // pass per rule. The program is ill-formed if a rule breaks on the
  // defaults.
  template<typename... R>
    requires(some_rule<R, Schema> && ...)
  constexpr std::array<bool, sizeof...(R)> fall_back_to_defaults(typelist<R...>)
  {
    static_assert(all_hold(typelist<R...>{}, image<Schema>{}),
                  "a rule must hold on the defaults");

    std::array<bool, sizeof...(R)> fell_back{};
    for (auto pass = 0uz; pass <= sizeof...(R); ++pass) {
      std::array<bool, sizeof...(R)> const broken{!R::holds(applied_)...};
      if (std::ranges::none_of(broken, std::identity{})) break;

      unroll<sizeof...(R)>([&]<std::size_t I>() {
        if (!broken[I]) return;
        using rule = typelist_at_t<typelist<R...>, I>;
        for (auto const i : rule::in::indices) {
          applied_.assign_cell(i, Schema.parameters[i].def);
        }
        fell_back[I] = true;
      });
    }
    return fell_back;
  }

  // -- The images --

  // Returns the values requested, which writes change.
  constexpr image<Schema> const& requested() const
  {
    return requested_;
  }

  // Returns the values applied, out of which the application builds the
  // state it derives from the parameters.
  constexpr image<Schema> const& applied() const
  {
    return applied_;
  }

  // Returns the values the next startup would load, which before `load` are
  // taken to be the defaults.
  constexpr image<Schema> const& stored() const
  {
    return stored_;
  }

  // Returns the value applied of the parameter at `index`, as
  // `image::get_at` does.
  constexpr std::expected<value, error> applied_at(std::size_t index) const
  {
    return applied_.get_at(index);
  }

  // Returns the value that the next startup would load for the parameter at
  // `index`, as `image::get_at` does.
  constexpr std::expected<value, error> stored_at(std::size_t index) const
  {
    return stored_.get_at(index);
  }

  // -- Access by declaration --

  // Returns the value requested of the parameter that `Decl` declares, as
  // `image::get` does.
  template<auto& Decl>
    requires some_declaration<decltype(Decl)>
  constexpr typename parameter<Schema, Decl>::type get() const
  {
    return requested_.template get<Decl>();
  }

  // Writes `v` to the parameter that `Decl` declares whether or not it is
  // `writable`, as `image::set` does.
  template<auto& Decl>
    requires some_declaration<decltype(Decl)>
  constexpr std::expected<void, error>
  set(typename parameter<Schema, Decl>::type const& v)
  {
    return requested_.template set<Decl>(v);
  }

  // -- Access by index --

  constexpr std::expected<value, error> get_at(std::size_t index) const
  {
    return requested_.get_at(index);
  }

  // Writes `v` to the parameter at `index`, as `image::set_at` does; unlike
  // `set`, refuses a parameter that is not `writable`.
  constexpr std::expected<void, error> set_at(std::size_t index, value const& v)
  {
    return requested_.set_at(index, v);
  }

  // Restores the default of every parameter whose `writable` is `true`, as
  // `image::restore_default_at` does; the others keep their values.
  constexpr void restore_all_defaults()
  {
    for (auto i = 0uz; i < count; ++i) {
      auto _ = requested_.restore_default_at(i);
    }
  }

private:
  static constexpr bool applies_on_restart(std::size_t index)
  {
    return Schema.parameters[index].apply == apply_policy::on_restart;
  }

  // Checks whether `a` and `b` differ in the cell of the parameter at
  // `index`.
  static constexpr bool
  differs(image<Schema> const& a, image<Schema> const& b, std::size_t index)
  {
    return a.cell_at(index) != b.cell_at(index);
  }

  // Returns the parameters applied on command whose value requested differs
  // from the one applied.
  constexpr selection pending() const
  {
    selection chosen;
    for (auto i = 0uz; i < count; ++i) {
      chosen.set(i, !applies_on_restart(i) && differs(requested_, applied_, i));
    }
    return chosen;
  }

  // Keeps the refusal for `reason` as `last_refusal()` and returns it.
  constexpr std::unexpected<apply_refusal> refuse(apply_refusal reason)
  {
    last_refusal_ = reason;
    return std::unexpected(reason);
  }

  // Keeps the refusal of the save that `failure` tells of as `last_refusal()`
  // and returns it.
  constexpr std::unexpected<section_save_failure<error_type>>
  refuse(section_save_failure<error_type> const& failure)
  {
    last_refusal_ = failure.reason;
    return std::unexpected(failure);
  }

  // Applies the parameters `chosen`, as `apply_all` describes.
  template<typename A>
  constexpr std::expected<void, apply_refusal> commit(selection const& chosen,
                                                      A& applier)
  {
    if (chosen.none()) return {};

    image<Schema> candidate = applied_;
    for (auto i = 0uz; i < count; ++i) {
      if (chosen.test(i)) candidate.assign_cell(i, requested_.cell_at(i));
    }
    if (!applier.check(std::as_const(candidate))) {
      return refuse(apply_refusal::invalid);
    }

    [[maybe_unused]] auto const guard = applier.lock();
    if (!applier.admit(std::as_const(applied_), std::as_const(candidate))) {
      return refuse(apply_refusal::not_admitted);
    }
    auto const before = std::exchange(applied_, candidate);
    applier.configure(before, std::as_const(applied_));
    return {};
  }
};

} // namespace settings
} // namespace emb
