#pragma once

#include <emb/assert.hpp>
#include <emb/meta/fixed_string.hpp>
#include <emb/nvm/storage.hpp>
#include <emb/settings/image.hpp>
#include <emb/settings/param.hpp>
#include <emb/settings/schema.hpp>
#include <emb/settings/store.hpp>

#include <expected>
#include <optional>

#include <cstddef>
#include <cstdint>

namespace emb {
namespace settings {

// The class template `section` binds the schema `Schema` to the place
// `Placement` on a medium of type `Storage`. It holds the image of the
// parameters, loads it from the medium and saves it there, and keeps two more
// images to compare it with: the values the next startup would load, and the
// values applied, from which the application last built the state it derives
// from the parameters. A write through `set`, `set_at`,
// `restore_default_at` or `restore_all_defaults` changes the image only;
// `load` sets all three images, `take` records the values of one group as
// applied, and `adopt` writes a value and records it as applied. `unsaved`
// and `unapplied` compare the image with the other two cell by cell, i.e. by
// encoding, so a value written and then put back leaves nothing to save or to
// apply; `-0.0f` does not put back `+0.0f`.
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

  image<Schema> values_;
  image<Schema> stored_;
  image<Schema> applied_;
  load_result last_load_;
  std::uint32_t sequence_ = 0;
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
  // does. After a successful write, `unsaved()` is `false` until a value
  // changes, and `sequence()` returns the number of the record written. The
  // values applied are left as they are.
  constexpr std::expected<void, save_failure<error_type>> save()
  {
    ASSUME(store_.has_value());
    return store_->save(values_).transform([this](save_result const& saved) {
      stored_ = values_;
      sequence_ = saved.seq;
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
    return differ(values_, stored_);
  }

  // Checks whether a parameter holds a value other than the one applied.
  constexpr bool unapplied() const
  {
    return differ(values_, applied_);
  }

  // Checks whether a parameter whose apply policy is `apply` holds a value
  // other than the one applied.
  constexpr bool unapplied(apply_policy apply) const
  {
    for (auto i = 0uz; i < image<Schema>::count; ++i) {
      if ((values_.cell(i) != applied_.cell(i))
          && (Schema.parameters[i].apply == apply)) {
        return true;
      }
    }
    return false;
  }

  // Checks whether a parameter whose apply policy is
  // `apply_policy::on_restart` holds a value other than the one applied,
  // i.e. returns `unapplied(apply_policy::on_restart)`. Each such value keeps
  // it so until a `load`, a `take` of its group whose `up_to` is
  // `apply_policy::on_restart`, an `adopt` of it, or a write that puts its
  // applied value back.
  constexpr bool restart_required() const
  {
    return unapplied(apply_policy::on_restart);
  }

  // If a parameter of `group` holds a value other than the one applied, and
  // the apply policy of every such parameter is `up_to` or a less demanding
  // one, records the values of `group` as applied and returns `true`; the
  // caller then rebuilds the state it derives from `group` from the image.
  // Otherwise returns `false` and there are no effects. `up_to` is the most
  // demanding policy the caller can honour at the call.
  //
  // A group is applied whole: one that also holds an unapplied value under a
  // more demanding policy is refused, its values under `up_to` included. A
  // value written to a parameter of `group` after `take` and before the
  // caller reads the image reaches the caller whatever its policy, and stays
  // unapplied for the next `take`; to exclude that, writes to the parameters
  // of `group` must not run between the two.
  [[nodiscard]] constexpr bool take(group_id group, apply_policy up_to)
  {
    auto differs = false;
    for (auto i = 0uz; i < image<Schema>::count; ++i) {
      if (values_.cell(i) == applied_.cell(i)) continue;
      auto const& desc = Schema.parameters[i];
      if (desc.group != group) continue;
      if (desc.apply > up_to) return false;
      differs = true;
    }
    if (!differs) return false;

    for (auto i = 0uz; i < image<Schema>::count; ++i)
      if (Schema.parameters[i].group == group)
        applied_.assign_cell(i, values_.cell(i));
    return true;
  }

  // -- Access by name --

  template<fixed_string Name>
  constexpr typename parameter<Schema, Name>::type get() const
  {
    return values_.template get<Name>();
  }

  // Writes `v` to the parameter `Name` whether or not it is `writable`, as
  // `image::set` does.
  template<fixed_string Name>
  constexpr std::expected<void, error>
  set(typename parameter<Schema, Name>::type const& v)
  {
    return values_.template set<Name>(v);
  }

  // Writes `v` to the parameter `Name`, as `set` does, and records it as
  // applied: for a value that the code owning the parameter has already
  // applied itself, e.g. a calibration result that a sensor took when it was
  // computed. A `take` of its group then finds nothing to apply in it, and
  // `unapplied` does not report it; a `take` that another value of the group
  // calls for still has the caller rebuild from it, so the owner adopts the
  // value before it takes the group.
  template<fixed_string Name>
  constexpr std::expected<void, error>
  adopt(typename parameter<Schema, Name>::type const& v)
  {
    using param_type = parameter<Schema, Name>;
    return values_.template set<Name>(v).transform([this] {
      applied_.assign_cell(param_type::index, values_.cell(param_type::index));
    });
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

  // Restores the default of the parameter at `index`, as
  // `image::restore_default_at` does, which refuses a parameter that is not
  // `writable`.
  constexpr std::expected<void, error> restore_default_at(std::size_t index)
  {
    return values_.restore_default_at(index);
  }

  // Restores the default of every parameter whose `writable` is `true`, as
  // `restore_default_at` does; the others keep their values.
  constexpr void restore_all_defaults()
  {
    for (auto i = 0uz; i < image<Schema>::count; ++i) {
      auto _ = values_.restore_default_at(i);
    }
  }

private:
  // Checks whether `a` and `b` differ in the cell of any parameter.
  static constexpr bool differ(image<Schema> const& a, image<Schema> const& b)
  {
    for (auto i = 0uz; i < image<Schema>::count; ++i)
      if (a.cell(i) != b.cell(i)) return true;
    return false;
  }
};

} // namespace settings
} // namespace emb
