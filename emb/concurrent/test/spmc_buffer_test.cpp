#include <emb/concurrent/spmc_buffer.hpp>

#include <concepts>
#include <cstdint>
#include <type_traits>
#include <utility>

namespace {

struct sample {
  float a;
  float b;
  float c;
};

static_assert(
    std::same_as<emb::spmc_buffer<sample, 2>,
                 emb::spmc_buffer<sample, 2, emb::memory_scope::smp>>);
static_assert(
    std::same_as<emb::local_spmc_buffer<sample, 2>,
                 emb::spmc_buffer<sample, 2, emb::memory_scope::local>>);
static_assert(!std::is_copy_constructible_v<emb::spmc_buffer<sample, 2>>);
static_assert(sizeof(emb::spmc_buffer<sample, 2>)
              == 4 * sizeof(sample) + 5 * sizeof(std::uint32_t));
static_assert(sizeof(emb::spmc_buffer<sample, 1>)
              == 3 * sizeof(sample) + 4 * sizeof(std::uint32_t));

template<typename Buffer>
void exercise(Buffer& buffer)
{
  buffer.store(sample{1.0f, 2.0f, 3.0f});
  [[maybe_unused]] auto const got = std::as_const(buffer).load();
}

// Both scopes compile here, whichever of them the firmware uses.
[[maybe_unused]] void instantiate_spmc_buffer()
{
  emb::spmc_buffer<sample, 2> smp;
  emb::local_spmc_buffer<sample, 1> local;
  exercise(smp);
  exercise(local);
}

} // namespace
