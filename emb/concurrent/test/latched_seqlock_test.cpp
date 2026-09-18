#include <emb/concurrent/latched_seqlock.hpp>

#include <concepts>
#include <cstdint>
#include <type_traits>

namespace {

struct sample {
  float a;
  float b;
  float c;
};

static_assert(
    std::same_as<emb::latched_seqlock<sample>,
                 emb::latched_seqlock<sample, emb::memory_scope::smp>>);
static_assert(
    std::same_as<emb::local_latched_seqlock<sample>,
                 emb::latched_seqlock<sample, emb::memory_scope::local>>);
static_assert(!std::is_copy_constructible_v<emb::latched_seqlock<sample>>);
static_assert(sizeof(emb::latched_seqlock<sample>)
              == sizeof(std::uint32_t) + 2 * sizeof(sample));

template<typename Latch>
void exercise(Latch& latch)
{
  latch.store(sample{1.0f, 2.0f, 3.0f});
  latch.update([](sample const& v) { return sample{v.c, v.b, v.a}; });
  [[maybe_unused]] auto const got = latch.load();
}

// No firmware code uses either scope, so this is where both compile.
[[maybe_unused]] void instantiate_latched_seqlock()
{
  emb::latched_seqlock<sample> smp;
  emb::local_latched_seqlock<sample> local;
  exercise(smp);
  exercise(local);
}

} // namespace
