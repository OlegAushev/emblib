#pragma once

#include <emb/settings/param.hpp>
#include <emb/settings/schema.hpp>

#include <array>
#include <atomic>
#include <optional>

#include <cstddef>
#include <cstdint>

namespace emb {
namespace settings {

namespace detail {

// Returns the mask in which only the bit of `g` is set, i.e. bit `g.value`.
// The behavior is undefined if `g.value >= 32`.
constexpr std::uint32_t group_bit(group_id g)
{
  return std::uint32_t{1} << g.value;
}

// Returns the index of the mask that holds the marks under `p`, i.e. the
// value of `p`. `basic_pending_changes` relies on the enumerators of
// `apply_policy` being ordered from the least to the most demanding, so that
// the policies up to `p` index a prefix of the masks: reordering them breaks
// it, and adding one needs another mask.
constexpr std::size_t policy_index(apply_policy p)
{
  return static_cast<std::size_t>(p);
}

} // namespace detail

// The class template `basic_pending_changes` records which groups have
// changes not applied yet, in one mask of groups per apply policy. A group is
// marked under the apply policy of a change after the change has been
// written to the image; the owner of the state derived from the group calls
// `take` and, if it returns `true`, rebuilds that state from the image.
//
// `Word` must provide `load`, `fetch_or` and `fetch_and` as
// `std::atomic<std::uint32_t>` does, and hold zero when value-initialized.
// With the default `Word`, which is lock-free, every member function may be
// called concurrently from interrupt handlers and tasks, and what precedes a
// `mark` is visible to a caller that observes the mark. `take`, `changed`,
// `any` and `clear` access the masks one at a time and are not atomic as a
// whole. With a non-atomic `Word`, e.g. the plain word that lets tests run
// in constant expressions, concurrent calls are data races.
template<typename Word = std::atomic<std::uint32_t>>
class basic_pending_changes {
  std::array<Word, 3> masks_{};

public:
  // Number of groups the masks can track. The behavior of `mark`, `take` and
  // `changed` is undefined for a group whose `value` is not less than
  // `group_limit`.
  static constexpr std::size_t group_limit = 32;

  constexpr void mark(group_id group, apply_policy apply)
  {
    masks_[detail::policy_index(apply)].fetch_or(detail::group_bit(group),
                                                 std::memory_order_acq_rel);
  }

  constexpr void mark(change c)
  {
    mark(c.group, c.apply);
  }

  // Marks the group of `*c` under its apply policy if `c` contains a change,
  // e.g. the result of a write to the image that changed a value; otherwise
  // there are no effects.
  constexpr void mark(std::optional<change> c)
  {
    if (c) mark(*c);
  }

  // Clears the marks of `group` under `up_to` and the less demanding apply
  // policies, and returns whether any of them was set. `up_to` is the most
  // demanding policy the caller can honour at the call. If `group` also has
  // a mark under a more demanding policy, returns `false` and there are no
  // effects.
  //
  // A change marked after `take` has cleared its mark stays marked for the
  // next `take`, even if the caller has yet to read the image; testing the
  // marks and clearing them in separate steps would lose it. A change under a
  // more demanding policy that is not marked yet when `take` checks the marks
  // can be in the image the caller reads, and its mark stays. To exclude
  // that, writes to the parameters of `group` must not run between `take` and
  // the caller's reading of the image.
  constexpr bool take(group_id group, apply_policy up_to)
  {
    if (blocked_above(group, up_to)) return false;

    auto const bit = detail::group_bit(group);
    bool taken = false;
    for (auto i = 0uz; i <= detail::policy_index(up_to); ++i) {
      auto const before = masks_[i].fetch_and(~bit, std::memory_order_acq_rel);
      taken = taken || ((before & bit) != 0);
    }
    return taken;
  }

  // Checks whether `group` has a mark under `up_to` or a less demanding apply
  // policy. Unlike `take`, clears nothing and ignores marks under more
  // demanding policies, so it can return `true` for a group that `take`
  // refuses.
  constexpr bool changed(group_id group, apply_policy up_to) const
  {
    auto const bit = detail::group_bit(group);
    for (auto i = 0uz; i <= detail::policy_index(up_to); ++i)
      if ((masks_[i].load(std::memory_order_acquire) & bit) != 0) return true;
    return false;
  }

  // Returns the groups that have a mark under `apply`, as a mask in which bit
  // n stands for the group whose `value` is n.
  constexpr std::uint32_t mask(apply_policy apply) const
  {
    return masks_[detail::policy_index(apply)].load(std::memory_order_acquire);
  }

  // Checks whether any group has a mark under `apply_policy::on_restart`.
  // Such a mark stays until `clear()` or a `take` whose `up_to` is
  // `apply_policy::on_restart` removes it.
  constexpr bool restart_required() const
  {
    return mask(apply_policy::on_restart) != 0;
  }

  // Checks whether any group has a mark under any apply policy.
  constexpr bool any() const
  {
    for (auto const& m : masks_)
      if (m.load(std::memory_order_acquire) != 0) return true;
    return false;
  }

  constexpr void clear()
  {
    for (auto& m : masks_)
      m.fetch_and(0u, std::memory_order_acq_rel);
  }

private:
  // Checks whether `group` has a mark under an apply policy more demanding
  // than `up_to`.
  constexpr bool blocked_above(group_id group, apply_policy up_to) const
  {
    auto const bit = detail::group_bit(group);
    for (auto i = detail::policy_index(up_to) + 1; i < masks_.size(); ++i)
      if ((masks_[i].load(std::memory_order_acquire) & bit) != 0) return true;
    return false;
  }
};

// `pending_changes` is `basic_pending_changes` with the default `Word`,
// `std::atomic<std::uint32_t>`.
using pending_changes = basic_pending_changes<>;

static_assert(std::atomic<std::uint32_t>::is_always_lock_free,
              "pending_changes is shared between an interrupt and a task");

// Checks whether `basic_pending_changes` can track the group of every
// parameter of `Schema`, i.e. whether every group's `value` is less than
// `pending_changes::group_limit`.
template<auto& Schema>
consteval bool groups_fit()
{
  for (auto const& p : Schema.parameters)
    if (p.group.value >= pending_changes::group_limit) return false;
  return true;
}

} // namespace settings
} // namespace emb
