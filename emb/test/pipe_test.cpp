#include <emb/pipe.hpp>

#include <cassert>
#include <functional>
#include <type_traits>

// deliberately outside emb and without a single using-declaration: a chain has
// to compile on the strength of the steps alone
namespace {

using emb::pipe::fn;
using emb::pipe::store;
using emb::pipe::tap;
using emb::pipe::with;

struct twice : emb::pipe::pipeable<twice> {
  static constexpr float operator()(float x)
  {
    return x * 2.f;
  }
};

struct add_one : emb::pipe::pipeable<add_one> {
  static constexpr float operator()(float x)
  {
    return x + 1.f;
  }
};

struct to_int : emb::pipe::pipeable<to_int> {
  static constexpr int operator()(float x)
  {
    return static_cast<int>(x);
  }
};

// a callable that is not a step, and the same thing lifted into one
struct plain {
  constexpr float operator()(float x) const
  {
    return x + 100.f;
  }
};

template<typename T, typename P>
concept pipes_into = requires(T t, P p) { t | p; };

// ---- what may be piped -----------------------------------------------------

static_assert(emb::pipe::some_pipeable<twice>);
static_assert(!emb::pipe::some_pipeable<plain>);

static_assert(pipes_into<float, twice>);
static_assert(pipes_into<float, decltype(fn(plain{}))>);

// nothing is pipeable by accident, and `|` still means what it always meant
static_assert(!pipes_into<float, plain>);
static_assert((1 | 2) == 3);

// ---- what a step accepts ---------------------------------------------------

static_assert(emb::pipe::pipeable_for<to_int, float>);
static_assert(!emb::pipe::pipeable_for<to_int, char const*>);
static_assert(std::is_same_v<emb::pipe::output_t<to_int, float>, int>);

// ---- composition -----------------------------------------------------------

// a chain of steps is a step, flattened into one type rather than nested ones
static_assert(emb::pipe::some_pipeable<decltype(twice{} | add_one{})>);
static_assert(std::is_same_v<decltype(twice{} | add_one{} | to_int{}),
                             emb::pipe::composed<twice, add_one, to_int>>);
static_assert(std::is_same_v<decltype((twice{} | add_one{}) | to_int{}),
                             decltype(twice{} | (add_one{} | to_int{}))>);

// and steps that carry nothing compose into a chain that costs nothing to
// carry: the distinct pipeable<Self> bases let the empty steps overlap
static_assert(sizeof(twice{} | add_one{} | to_int{}) == 1);

constexpr bool test_composition()
{
  assert((3.f | twice{}) == 6.f);
  assert((3.f | (twice{} | add_one{})) == 7.f);

  // composition is associative and the order is left to right
  assert((3.f | (twice{} | add_one{} | to_int{})) == 7);
  assert((3.f | (add_one{} | twice{} | to_int{})) == 8);

  // a pipeline is a value: name it once, apply it twice
  [[maybe_unused]] constexpr auto pipeline = twice{} | add_one{};
  assert(pipeline(1.f) == 3.f);
  assert((1.f | pipeline) == 3.f);
  assert((10.f | pipeline) == 21.f);

  return true;
}

// ---- lifting ---------------------------------------------------------------

constexpr bool test_fn()
{
  assert((1.f | fn(plain{})) == 101.f);
  assert((1.f | fn([](float x) { return x * 3.f; })) == 3.f);
  assert((1.f | (twice{} | fn(plain{}))) == 102.f);

  // a step may change the type the chain carries
  assert((2.5f | fn([](float x) { return static_cast<int>(x) * 2; })) == 4);

  return true;
}

// ---- side effects ----------------------------------------------------------

constexpr bool test_store_and_tap()
{
  [[maybe_unused]] float seen = 0.f;
  [[maybe_unused]] float kept = 0.f;

  [[maybe_unused]] float const out = 3.f
                  | twice{}
                  | store(kept)
                  | tap([&](float x) { seen = x + 1.f; })
                  | add_one{};

  assert(out == 7.f);
  assert(kept == 6.f);  // what passed, unchanged
  assert(seen == 7.f);  // the observer saw the same value

  // store writes wherever the value is assignable, not only to its own type
  [[maybe_unused]] double wide = 0.;
  assert((2.f | store(wide)) == 2.f);
  assert(wide == 2.);

  return true;
}

// ---- bound arguments -------------------------------------------------------

constexpr float scale(float x, float k)
{
  return x * k;
}

constexpr bool test_with()
{
  // by value: the argument is copied where with() is written
  {
    float k = 2.f;
    [[maybe_unused]] auto const step = with(scale, k);
    k = 10.f;
    assert((3.f | step) == 6.f);
  }

  // std::ref: the argument is read at the moment the step runs
  {
    float k = 2.f;
    [[maybe_unused]] auto const step = with(scale, std::ref(k));
    k = 10.f;
    assert((3.f | step) == 30.f);
  }

  // more than one bound argument, in the order written
  assert((1.f | with([](float x, float a, float b) { return (x + a) * b; },
                     2.f,
                     10.f))
         == 30.f);

  return true;
}

static_assert(test_composition());
static_assert(test_fn());
static_assert(test_store_and_tap());
static_assert(test_with());

} // namespace
