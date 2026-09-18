#include <emb/concurrent/seqlock.hpp>

#include <concepts>
#include <type_traits>

namespace {

struct sample {
  float a;
  float b;
  float c;
};

static_assert(std::same_as<emb::seqlock<sample>,
                           emb::seqlock<sample, emb::memory_scope::smp>>);
static_assert(std::same_as<emb::local_seqlock<sample>,
                           emb::seqlock<sample, emb::memory_scope::local>>);
static_assert(!std::is_copy_constructible_v<emb::seqlock<sample>>);

template<typename Lock>
void exercise(Lock& lock)
{
  lock.store(sample{1.0f, 2.0f, 3.0f});
  lock.update([](sample const& v) { return sample{v.c, v.b, v.a}; });
  [[maybe_unused]] auto const got = lock.load();
}

// No firmware code uses either scope, so this is where both compile.
[[maybe_unused]] void instantiate_seqlock()
{
  emb::seqlock<sample> smp;
  emb::local_seqlock<sample> local;
  exercise(smp);
  exercise(local);
}

} // namespace
