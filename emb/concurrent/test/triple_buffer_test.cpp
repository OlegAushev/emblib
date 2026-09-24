#include <emb/concurrent/triple_buffer.hpp>

#include <concepts>

namespace {

struct sample {
  float a;
  float b;
  float c;
};

static_assert(std::same_as<emb::triple_buffer<sample>,
                           emb::triple_buffer<sample, emb::memory_scope::smp>>);
static_assert(
    std::same_as<emb::local_triple_buffer<sample>,
                 emb::triple_buffer<sample, emb::memory_scope::local>>);
static_assert(sizeof(emb::local_triple_buffer<sample>)
              == sizeof(emb::triple_buffer<sample>));

template<typename Buffer>
void exercise(Buffer& buffer)
{
  buffer.store(sample{1.0f, 2.0f, 3.0f});
  [[maybe_unused]] auto const got = buffer.load();
}

// Both scopes compile here, whichever of them the firmware uses.
[[maybe_unused]] void instantiate_triple_buffer()
{
  emb::triple_buffer<sample> smp;
  emb::local_triple_buffer<sample> local;
  exercise(smp);
  exercise(local);
}

} // namespace
