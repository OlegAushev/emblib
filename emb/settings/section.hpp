#pragma once

#include <emb/assert.hpp>
#include <emb/nvm/storage.hpp>
#include <emb/settings/image.hpp>
#include <emb/settings/param.hpp>
#include <emb/settings/schema.hpp>
#include <emb/settings/store.hpp>

#include <algorithm>
#include <array>
#include <bitset>
#include <concepts>
#include <expected>
#include <optional>
#include <utility>

#include <cstddef>
#include <cstdint>

namespace emb {
namespace settings {

// The scoped enumeration `apply_refusal` defines why `section::apply`,
// `section::apply_group` or `section::apply_all` applied nothing.
enum class apply_refusal : std::uint8_t {
  // Index that names no parameter, returned by `section::apply`.
  unknown_parameter,
  // A parameter whose apply policy is `apply_policy::on_restart`, or a group
  // in which the schema declares no parameter applied on command: nothing
  // applies it on command, whatever its values.
  not_applicable,
  // The checker refused the values that a group would take.
  invalid,
  // The applier did not admit a change to a group in its present state.
  not_admitted,
};

// The struct `apply_failure` tells why a section applied nothing, and for
// which group: the group refused, or that of what is not applicable.
struct apply_failure {
  apply_refusal reason;
  group_id group;

  friend constexpr bool operator==(apply_failure, apply_failure) = default;
};

// The scoped enumeration `save_refusal` defines why `section::save` wrote
// nothing, or failed to.
enum class save_refusal : std::uint8_t {
  // A parameter applied on command holds a value other than the one applied:
  // a save keeps what the machine runs on, not what waits to be applied.
  unapplied,
  // The checker refused the values that a group, waiting for a restart,
  // would take at the next startup.
  invalid,
  // The store failed to write the record.
  medium,
};

// The class template `section_save_failure` tells why `section::save` wrote
// nothing or failed to: the `reason`, the `group` that the checker refused,
// and for `save_refusal::medium` what the store reported, `store`.
template<typename Error>
struct section_save_failure {
  save_refusal reason;
  group_id group{};
  std::optional<save_failure<Error>> store{};
};

// The concept `some_checker<C, Schema>` is satisfied if and only if `C`
// checks the values of a group as a whole: `checker.check(group, values)`
// returns whether the parameters of `group` may take their values in
// `values`, an image of `Schema`, together, which the bounds of each alone
// cannot tell. A check depends on the values alone, so its answer holds
// wherever it is asked: before a section applies the values, before it saves
// them, and when the application builds its state after a load.
template<typename C, auto& Schema>
concept some_checker =
    requires(C& checker, group_id group, image<Schema> const& values) {
      { checker.check(group, values) } -> std::same_as<bool>;
    };

// The concept `some_applier<A, Schema>` is satisfied if and only if `A` is a
// checker that also brings the state the application derives from the
// parameters to the values applied: `applier.lock()` returns a guard, an
// object that holds that state still for as long as it lives, whether or not
// it can be copied or moved; `applier.admit(group)` returns whether a change
// to `group` may take effect in the present state; and
// `applier.configure(group, applied)` rebuilds what derives from `group` out
// of `applied`, the values applied, and cannot fail: whatever may refuse is
// asked before, by `check` or `admit`.
template<typename A, auto& Schema>
concept some_applier =
    some_checker<A, Schema>
    && requires(A& applier, group_id group, image<Schema> const& applied) {
         applier.lock();
         { applier.admit(group) } -> std::same_as<bool>;
         applier.configure(group, applied);
       };

// The class template `section` binds the schema `Schema` to the place
// `Placement` on a medium of type `Storage`. It holds the image of the
// parameters, the values requested, loads it from the medium and saves it
// there, and keeps two more images: the values the next startup would load,
// and the values applied, out of which the application builds the state it
// derives from the parameters. A write through `set`, `set_at` or
// `restore_all_defaults` changes the values requested only. `load` sets all
// three images; `apply`, `apply_group` and `apply_all` record values
// requested as applied, once an applier has checked them, admitted them and
// rebuilt its state from them; `discard`, `discard_group` and `discard_all`
// put the values applied back into the values requested. `unsaved`,
// `unapplied` and `restart_required` compare the images cell by cell, i.e. by
// encoding, so a value written and then put back leaves nothing to save or to
// apply; `-0.0f` does not put back `+0.0f`. `values`, `applied` and `stored`
// return the three images.
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
  static constexpr auto& groups = groups_of<Schema>;

  // Cells, by index, that a call applies.
  using selection = std::bitset<count>;
  // Groups, by position in `groups`, that a call checks.
  using group_set = std::bitset<groups.size()>;

  // The position in `groups` of the group of each parameter.
  static constexpr auto group_slots = [] {
    std::array<std::size_t, count> slots{};
    for (auto i = 0uz; i < count; ++i) {
      while (groups[slots[i]] != Schema.parameters[i].group) ++slots[i];
    }
    return slots;
  }();

  image<Schema> values_;
  image<Schema> stored_;
  image<Schema> applied_;
  load_result last_load_;
  std::uint32_t sequence_ = 0;
  std::optional<apply_failure> last_failure_;
  std::optional<store_type> store_;

public:
  using storage_type = Storage;

  static constexpr auto& schema = Schema;
  static constexpr auto& placement = Placement;

  // Binds the section to `medium` and restores the image from the newest
  // whole record there, or from the defaults if there is none, as
  // `store::load` does, and takes what it restored as applied: the
  // application builds the state it derives from the parameters after the
  // load. Returns what the load found, which `last_load()` keeps. The section
  // keeps a reference to `medium` for `save` and `wipe` until a later call to
  // `load` replaces it.
  constexpr load_result load(Storage& medium)
  {
    store_.emplace(medium);
    last_load_ = store_->load(values_);
    stored_ = values_;
    applied_ = values_;
    sequence_ = last_load_.record.seq;
    return last_load_;
  }

  constexpr load_result const& last_load() const
  {
    return last_load_;
  }

  // Writes the image to the medium as the next record, as `store::save`
  // does, if every parameter applied on command holds its value applied and
  // `checker` accepts the values of every group with a parameter that waits
  // for a restart: the next startup then loads what the machine runs on, and
  // what was checked for the restart. A group the checker refuses is kept as
  // `last_failure()`. After a successful write, `unsaved()` is `false` until
  // a value changes, and `sequence()` returns the number of the record
  // written. The values applied are left as they are.
  template<some_checker<Schema> C>
  constexpr std::expected<void, section_save_failure<error_type>>
  save(C& checker)
  {
    ASSUME(store_.has_value());
    using failure = section_save_failure<error_type>;

    if (unapplied()) return std::unexpected(failure{save_refusal::unapplied});

    group_set waiting;
    for (auto i = 0uz; i < count; ++i) {
      if (restarts_with(i) && differs(values_, applied_, i)) {
        waiting.set(group_slots[i]);
      }
    }
    for (auto g = 0uz; g < groups.size(); ++g) {
      if (waiting.test(g) && !checker.check(groups[g], values_)) {
        last_failure_ = apply_failure{apply_refusal::invalid, groups[g]};
        return std::unexpected(failure{save_refusal::invalid, groups[g]});
      }
    }

    return store_->save(values_)
        .transform([this](save_result const& saved) {
          stored_ = values_;
          sequence_ = saved.seq;
        })
        .transform_error([](save_failure<error_type> const& cause) {
          return failure{save_refusal::medium, {}, cause};
        });
  }

  // Brings every slot of the section on the medium to the erased state, as
  // `store::wipe` does. The image and the values applied are left as they
  // are; after a successful wipe, `unsaved()` compares the image with the
  // defaults, which the next startup would load, and `sequence()` returns
  // zero.
  constexpr std::expected<void, error_type> wipe()
  {
    ASSUME(store_.has_value());
    auto const result = store_->wipe();
    if (result) {
      stored_ = image<Schema>{};
      sequence_ = 0;
    }
    return result;
  }

  // Returns the sequence number of the record that the section last restored
  // or wrote, or zero if there is none. `load` sets it to the number of the
  // record restored, or to zero if none was; a successful `save` sets it to
  // the number of the record written, and a successful `wipe` to zero; a
  // failed `save` or `wipe` leaves it unchanged. Before the first `load`, it
  // is zero.
  //
  // A save numbers its record from the newest whole record on the medium,
  // not from `sequence()`: with the number of that record plus the distance,
  // in slots along the ring, from that record to the slot the save takes, or
  // with one if the medium holds no whole record. The distance exceeds one by
  // the number of slots that the save stepped over because they were not
  // erased, e.g. because they hold the debris of failed saves.
  constexpr std::uint32_t sequence() const
  {
    return sequence_;
  }

  // Checks whether the image differs from what the next startup would load,
  // which before `load` is taken to be the defaults. A failed `save` or
  // `wipe` does not change what the image is compared with, even if it
  // changed the medium.
  constexpr bool unsaved() const
  {
    for (auto i = 0uz; i < count; ++i)
      if (differs(values_, stored_, i)) return true;
    return false;
  }

  // Checks whether a parameter applied on command holds a value other than
  // the one applied.
  constexpr bool unapplied() const
  {
    for (auto i = 0uz; i < count; ++i)
      if (!restarts_with(i) && differs(values_, applied_, i)) return true;
    return false;
  }

  // Checks whether a parameter of `group` applied on command holds a value
  // other than the one applied.
  constexpr bool unapplied(group_id group) const
  {
    for (auto i = 0uz; i < count; ++i) {
      if ((Schema.parameters[i].group == group)
          && !restarts_with(i)
          && differs(values_, applied_, i)) {
        return true;
      }
    }
    return false;
  }

  // Checks whether a restart would change a parameter applied only after a
  // restart: whether one holds a value other than the one applied, or the
  // next startup would load such a value. A value saved and then put back
  // still owes the restart that would load it, until it is saved again.
  constexpr bool restart_required() const
  {
    for (auto i = 0uz; i < count; ++i) {
      if (restarts_with(i)
          && (differs(values_, applied_, i) || differs(stored_, applied_, i))) {
        return true;
      }
    }
    return false;
  }

  // -- Applying --

  // Applies the value requested of the parameter at `index`, as `apply_all`
  // applies all of them, if it differs from the one applied. Refused with
  // `apply_refusal::unknown_parameter` if `index >= count`, and with
  // `apply_refusal::not_applicable` if the parameter is applied only after a
  // restart, whatever its value.
  template<some_applier<Schema> A>
  constexpr std::expected<void, apply_failure> apply(std::size_t index,
                                                     A& applier)
  {
    if (index >= count) {
      return std::unexpected(
          apply_failure{apply_refusal::unknown_parameter, {}});
    }
    if (restarts_with(index)) {
      return refuse(apply_refusal::not_applicable,
                    Schema.parameters[index].group);
    }
    selection chosen;
    chosen.set(index, differs(values_, applied_, index));
    return commit(chosen, applier);
  }

  // Applies the values requested of the parameters of `group`, as
  // `apply_all` applies all of them. Refused with
  // `apply_refusal::not_applicable` if the schema declares no parameter of
  // `group` applied on command, whatever the values.
  template<some_applier<Schema> A>
  constexpr std::expected<void, apply_failure> apply_group(group_id group,
                                                           A& applier)
  {
    bool const applicable = std::ranges::any_of(
        Schema.parameters,
        [group](descriptor const& desc) {
          return (desc.group == group)
              && (desc.apply == apply_policy::on_command);
        });
    if (!applicable) {
      return refuse(apply_refusal::not_applicable, group);
    }
    return commit(pending([group](descriptor const& desc) {
                    return desc.group == group;
                  }),
                  applier);
  }

  // Applies the value requested of every parameter applied on command that
  // differs from the one applied, all of them or none; a parameter applied
  // only after a restart keeps waiting for it. `applier` checks the values
  // that each group of a change would take, every group before anything
  // changes; then, under its lock, admits a change to each group; only then
  // are the values recorded as applied and every group configured out of
  // them, in the order of `groups_of`. A group that fails the check or is not
  // admitted refuses the whole, and is kept as `last_failure()`. With nothing
  // to apply, succeeds without calling `applier`.
  template<some_applier<Schema> A>
  constexpr std::expected<void, apply_failure> apply_all(A& applier)
  {
    return commit(pending([](descriptor const&) { return true; }), applier);
  }

  // Returns the last refusal of `apply`, `apply_group` or `apply_all` that
  // names a group, or that of `save` by the checker, or nothing if there was
  // none. A success leaves it as it was.
  constexpr std::optional<apply_failure> last_failure() const
  {
    return last_failure_;
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
    values_.assign_cell(index, applied_.cell(index));
    return {};
  }

  // Puts the values applied of the `writable` parameters of `group` back into
  // the values requested; the others keep theirs.
  constexpr void discard_group(group_id group)
  {
    put_back([group](descriptor const& desc) { return desc.group == group; });
  }

  // Puts the values applied of the `writable` parameters back into the
  // values requested; the others keep theirs.
  constexpr void discard_all()
  {
    put_back([](descriptor const&) { return true; });
  }

  // Records the defaults of `group` as its values applied and leaves its
  // values requested as they are: for an application that finds the values
  // loaded for `group` invalid and builds what derives from it out of the
  // defaults instead. The values loaded then wait, to be applied once
  // corrected, or discarded.
  constexpr void fall_back_to_defaults(group_id group)
  {
    for (auto i = 0uz; i < count; ++i) {
      auto const& desc = Schema.parameters[i];
      if (desc.group == group) {
        applied_.assign_cell(i, desc.def);
      }
    }
  }

  // -- The images --

  // Returns the image, i.e. the values requested, which writes change.
  constexpr image<Schema> const& values() const
  {
    return values_;
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
    return values_.template get<Decl>();
  }

  // Writes `v` to the parameter that `Decl` declares whether or not it is
  // `writable`, as `image::set` does.
  template<auto& Decl>
    requires some_declaration<decltype(Decl)>
  constexpr std::expected<void, error>
  set(typename parameter<Schema, Decl>::type const& v)
  {
    return values_.template set<Decl>(v);
  }

  // -- Access by index --

  constexpr std::expected<value, error> get_at(std::size_t index) const
  {
    return values_.get_at(index);
  }

  // Writes `v` to the parameter at `index`, as `image::set_at` does; unlike
  // `set`, refuses a parameter that is not `writable`.
  constexpr std::expected<void, error> set_at(std::size_t index, value const& v)
  {
    return values_.set_at(index, v);
  }

  // Restores the default of every parameter whose `writable` is `true`, as
  // `image::restore_default_at` does; the others keep their values.
  constexpr void restore_all_defaults()
  {
    for (auto i = 0uz; i < count; ++i) {
      auto _ = values_.restore_default_at(i);
    }
  }

private:
  static constexpr bool restarts_with(std::size_t index)
  {
    return Schema.parameters[index].apply == apply_policy::on_restart;
  }

  // Checks whether `a` and `b` differ in the cell of the parameter at
  // `index`.
  static constexpr bool
  differs(image<Schema> const& a, image<Schema> const& b, std::size_t index)
  {
    return a.cell(index) != b.cell(index);
  }

  // Returns the parameters applied on command whose descriptor satisfies
  // `pred` and whose value requested differs from the one applied.
  template<typename Pred>
  constexpr selection pending(Pred pred) const
  {
    selection chosen;
    for (auto i = 0uz; i < count; ++i) {
      chosen.set(i,
                 !restarts_with(i)
                     && pred(Schema.parameters[i])
                     && differs(values_, applied_, i));
    }
    return chosen;
  }

  // Puts the values applied of the `writable` parameters whose descriptor
  // satisfies `pred` back into the values requested.
  template<typename Pred>
  constexpr void put_back(Pred pred)
  {
    for (auto i = 0uz; i < count; ++i) {
      auto const& desc = Schema.parameters[i];
      if (desc.writable && pred(desc)) {
        values_.assign_cell(i, applied_.cell(i));
      }
    }
  }

  // Keeps the refusal of `group` for `reason` as `last_failure()` and returns
  // it.
  constexpr std::unexpected<apply_failure> refuse(apply_refusal reason,
                                                  group_id group)
  {
    last_failure_ = apply_failure{reason, group};
    return std::unexpected(*last_failure_);
  }

  // Applies the parameters `chosen`, as `apply_all` describes.
  template<typename A>
  constexpr std::expected<void, apply_failure>
  commit(selection const& chosen, A& applier)
  {
    if (chosen.none()) return {};

    group_set touched;
    image<Schema> candidate = applied_;
    for (auto i = 0uz; i < count; ++i) {
      if (chosen.test(i)) {
        touched.set(group_slots[i]);
        candidate.assign_cell(i, values_.cell(i));
      }
    }

    for (auto g = 0uz; g < groups.size(); ++g) {
      if (touched.test(g) && !applier.check(groups[g], candidate)) {
        return refuse(apply_refusal::invalid, groups[g]);
      }
    }

    [[maybe_unused]] auto const guard = applier.lock();
    for (auto g = 0uz; g < groups.size(); ++g) {
      if (touched.test(g) && !applier.admit(groups[g])) {
        return refuse(apply_refusal::not_admitted, groups[g]);
      }
    }
    applied_ = candidate;
    for (auto g = 0uz; g < groups.size(); ++g) {
      if (touched.test(g)) {
        applier.configure(groups[g], std::as_const(applied_));
      }
    }
    return {};
  }
};

} // namespace settings
} // namespace emb
