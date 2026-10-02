#pragma once

#include <emb/assert.hpp>
#include <emb/meta/fixed_string.hpp>
#include <emb/nvm/storage.hpp>
#include <emb/settings/image.hpp>
#include <emb/settings/pending.hpp>
#include <emb/settings/schema.hpp>
#include <emb/settings/store.hpp>

#include <atomic>
#include <expected>
#include <optional>

#include <cstddef>
#include <cstdint>

namespace emb {
namespace settings {

// The class template `section` binds the schema `Schema` to the place
// `Placement` on a medium of type `Storage`. It holds the image of the
// parameters, loads it from the medium and saves it there, and tracks in
// `pending()` which groups have changes not applied yet: a write through
// `set`, `set_at`, `restore_default_at` or `restore_all_defaults` that
// changes a value marks the group of the parameter under its apply policy.
// No other member function marks or clears `pending()`; in particular,
// `load` replaces the image without marking anything.
//
// A section can be constant-initialized and then holds the defaults in its
// image. The behavior is undefined if `save`, `wipe` or `sequence` is called
// before `load` has bound the section to a medium; the other member
// functions may be called before it.
//
// `Word` is the word type of `pending()`, as in `basic_pending_changes`.
// Apart from `pending()`, the section is not synchronized: a call to a
// non-const member function must not run concurrently with any other call
// on the section, except, with the default `Word`, a call on `pending()`.
template<auto& Schema,
         nvm::some_block_storage Storage,
         placement Placement,
         typename Word = std::atomic<std::uint32_t>>
class section {
  using store_type = store<Schema, Storage, Placement>;
  using error_type = typename Storage::error_type;

  static_assert(groups_fit<Schema>(),
                "every group of the schema must fit the pending masks");

  image<Schema> values_;
  image<Schema> stored_;
  basic_pending_changes<Word> pending_;
  load_result last_load_;
  std::optional<store_type> store_;

public:
  using storage_type = Storage;

  static constexpr auto& schema = Schema;
  static constexpr auto& placement = Placement;

  // Binds the section to `medium` and restores the image from the newest
  // whole record there, or from the defaults if there is none, as
  // `store::load` does. Returns what the load found, which `last_load()`
  // keeps. The section keeps a reference to `medium` for `save` and `wipe`
  // until a later call to `load` replaces it.
  constexpr load_result load(Storage& medium)
  {
    store_.emplace(medium);
    last_load_ = store_->load(values_);
    stored_ = values_;
    return last_load_;
  }

  constexpr load_result const& last_load() const
  {
    return last_load_;
  }

  // Writes the image to the medium as the next record, as `store::save`
  // does. After a successful write, `unsaved()` is `false` until a value
  // changes.
  constexpr std::expected<void, save_failure<error_type>> save()
  {
    ASSUME(store_.has_value());
    auto const result = store_->save(values_);
    if (result) stored_ = values_;
    return result;
  }

  // Brings every slot of the section on the medium to the erased state, as
  // `store::wipe` does. The image is left as it is; after a successful wipe,
  // `unsaved()` compares it with the defaults, which the next startup would
  // load.
  constexpr std::expected<void, error_type> wipe()
  {
    ASSUME(store_.has_value());
    auto const result = store_->wipe();
    if (result) stored_ = image<Schema>{};
    return result;
  }

  // Returns the sequence number of the last record that the section numbered
  // or found on the medium. `load` sets it to the number in the newest record
  // header, or to zero if there is none or if the medium refused a read and
  // no record was restored; a successful `wipe` sets it to zero. Every `save`
  // increments it and numbers its record with the result, whether or not the
  // write succeeds; after a load that refused a read and restored nothing,
  // the save first takes it from the headers.
  constexpr std::uint32_t sequence() const
  {
    ASSUME(store_.has_value());
    return store_->sequence();
  }

  constexpr basic_pending_changes<Word>& pending()
  {
    return pending_;
  }

  constexpr basic_pending_changes<Word> const& pending() const
  {
    return pending_;
  }

  // Checks whether the image differs from what the next startup would load,
  // which before `load` is taken to be the defaults. A failed `save` or
  // `wipe` does not change what the image is compared with, even if it
  // changed the medium.
  constexpr bool unsaved() const
  {
    for (auto i = 0uz; i < image<Schema>::count; ++i)
      if (values_.cell(i) != stored_.cell(i)) return true;
    return false;
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
    return values_.template set<Name>(v).transform(
        [this](std::optional<change> c) { pending_.mark(c); });
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
    return values_.set_at(index, v).transform(
        [this](std::optional<change> c) { pending_.mark(c); });
  }

  // Restores the default of the parameter at `index`, as
  // `image::restore_default_at` does, which refuses a parameter that is not
  // `writable`.
  constexpr std::expected<void, error> restore_default_at(std::size_t index)
  {
    return values_.restore_default_at(index).transform(
        [this](std::optional<change> c) { pending_.mark(c); });
  }

  // Restores the default of every parameter whose `writable` is `true`, as
  // `restore_default_at` does; the others keep their values.
  constexpr void restore_all_defaults()
  {
    for (auto i = 0uz; i < image<Schema>::count; ++i)
      if (auto const c = values_.restore_default_at(i)) pending_.mark(*c);
  }
};

} // namespace settings
} // namespace emb
