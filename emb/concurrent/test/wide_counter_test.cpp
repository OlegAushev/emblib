#include <emb/concurrent/wide_counter.hpp>

#include <concepts>
#include <type_traits>

namespace {

static_assert(std::same_as<emb::wide_counter<>,
                           emb::wide_counter<emb::memory_scope::smp>>);
static_assert(std::same_as<emb::local_wide_counter,
                           emb::wide_counter<emb::memory_scope::local>>);
static_assert(!std::is_copy_constructible_v<emb::wide_counter<>>);
static_assert(sizeof(emb::wide_counter<>) == 8);

// A counter usually lives in a static, which must not wait for dynamic
// initialization.
[[maybe_unused]] constinit emb::local_wide_counter constant_initialized;

template<typename Counter>
void exercise(Counter& counter)
{
  counter.increment();
  [[maybe_unused]] auto const got = counter.load();
}

// Both scopes compile here, whichever one the firmware uses.
[[maybe_unused]] void instantiate_wide_counter()
{
  emb::wide_counter<> smp;
  emb::local_wide_counter local;
  exercise(smp);
  exercise(local);
}

} // namespace
