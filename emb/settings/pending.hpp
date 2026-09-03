#pragma once

#include <emb/settings/param.hpp>
#include <emb/settings/schema.hpp>

#include <array>
#include <atomic>

#include <cstddef>
#include <cstdint>

namespace emb {
namespace settings {

namespace detail {

constexpr auto group_bit(group_id g) -> std::uint32_t
{
  return std::uint32_t{1} << g.value;
}

// Policies are ordered by how much a caller must be able to promise before
// a change may take effect, so "everything up to this policy" is a prefix
// of the masks.
constexpr auto policy_index(apply_policy p) -> std::size_t
{
  return static_cast<std::size_t>(p);
}

} // namespace detail

// Which groups have changes waiting, split by the policy that governs when
// they may be applied.
//
// A writer — the task that services a protocol — marks a group after it has
// updated the image. The owner of the derived state asks for what it can
// honour right now, which clears those bits in the same operation: a change
// that lands between the query and the reconfiguration sets the bit again
// and is applied on the next round. Testing first and clearing afterwards
// would drop exactly that change.
//
// The word type is a template parameter so the bit arithmetic can be tested
// in constant expressions, where std::atomic cannot go. Production uses the
// default; nothing else about the class changes.
template<typename Word = std::atomic<std::uint32_t>>
class basic_pending_changes {
  std::array<Word, 3> masks_{};

public:
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

  // Whether the group has changes this caller may apply, clearing exactly
  // those. `up_to` is what the caller can honour: a running drive takes
  // apply_policy::live, one in a state where reconfiguration is safe takes
  // on_safe_state and gets both.
  constexpr bool take(group_id group, apply_policy up_to)
  {
    auto const bit = detail::group_bit(group);
    bool taken = false;
    for (auto i = 0uz; i <= detail::policy_index(up_to); ++i) {
      auto const before = masks_[i].fetch_and(~bit, std::memory_order_acq_rel);
      taken = taken || ((before & bit) != 0);
    }
    return taken;
  }

  // Non-destructive, for reporting: what an operator is told is waiting.
  constexpr bool changed(group_id group, apply_policy up_to) const
  {
    auto const bit = detail::group_bit(group);
    for (auto i = 0uz; i <= detail::policy_index(up_to); ++i)
      if ((masks_[i].load(std::memory_order_acquire) & bit) != 0) return true;
    return false;
  }

  constexpr auto mask(apply_policy apply) const -> std::uint32_t
  {
    return masks_[detail::policy_index(apply)].load(
        std::memory_order_acquire);
  }

  // Set when a parameter that cannot be applied without a restart has been
  // written, and never cleared: the condition ends with the restart.
  constexpr bool restart_required() const
  {
    return mask(apply_policy::on_restart) != 0;
  }

  constexpr void clear()
  {
    for (auto& m : masks_)
      m.fetch_and(0u, std::memory_order_acq_rel);
  }
};

using pending_changes = basic_pending_changes<>;

static_assert(std::atomic<std::uint32_t>::is_always_lock_free,
              "pending_changes is shared between an interrupt and a task");

// Every group must fit the masks. The schema cannot check this itself — it
// knows nothing of how changes are tracked — so the application asserts it
// where the two meet.
template<auto& Schema>
consteval bool groups_fit()
{
  for (auto const& p : Schema.parameters)
    if (p.group.value >= pending_changes::group_limit) return false;
  return true;
}

} // namespace settings
} // namespace emb
