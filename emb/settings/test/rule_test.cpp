#include <array>
#include <cstddef>
#include <cstdint>

#include <emb/meta/typelist.hpp>
#include <emb/settings/rule.hpp>

namespace {

using namespace emb;
using namespace emb::settings;

inline constexpr auto limit_low =
    param("limit.low",
          std::uint32_t{10},
          {.title = {"Low limit", "Нижний предел"}});
inline constexpr auto limit_high =
    param("limit.high",
          std::uint32_t{20},
          {.title = {"High limit", "Верхний предел"}});
inline constexpr auto ctrl_gain =
    param("ctrl.gain", 0.5f, {.title = {"Gain", "Коэффициент усиления"}});
inline constexpr auto schema = make_schema(limit_low, limit_high, ctrl_gain);

inline constexpr auto other_x =
    param("other.x", std::uint32_t{0}, {.title = {"Other X", "Другой X"}});
inline constexpr auto other_schema = make_schema(other_x);

using limits = inputs<schema, limit_high, limit_low>;

// The indices of the inputs follow the order of the declarations given.
static_assert(limits::indices == std::array<std::size_t, 2>{1, 0});

// Whether `In` lets a function read the parameter that `Decl` declares.
template<typename In, auto& Decl>
concept reads = requires(In const in) { in.template get<Decl>(); };

// The inputs read their parameters, and neither another parameter of the
// schema nor one of another schema.
static_assert(reads<limits, limit_low>);
static_assert(reads<limits, limit_high>);
static_assert(!reads<limits, ctrl_gain>);
static_assert(!reads<limits, other_x>);

// An image converts to inputs, which read its values in their types.
consteval bool test_inputs_read_the_image()
{
  image<schema> values;
  if (!values.set<limit_low>(std::uint32_t{12})) return false;
  limits const in = values;
  return (in.get<limit_low>() == 12) && (in.get<limit_high>() == 20);
}

static_assert(test_inputs_read_the_image());

// limit.low lies below limit.high.
struct ordered {
  using in = limits;

  static constexpr bool holds(in v)
  {
    return v.get<limit_low>() < v.get<limit_high>();
  }
};

// ctrl.gain is positive.
struct positive {
  using in = inputs<schema, ctrl_gain>;

  static constexpr bool holds(in v)
  {
    return v.get<ctrl_gain>() > 0.0f;
  }
};

// Not rules: `holds` returns no bool, or there are no inputs, or they are
// inputs of another schema.
struct counting {
  using in = limits;

  static constexpr int holds(in)
  {
    return 1;
  }
};

struct blind {
  static constexpr bool holds(image<schema> const&)
  {
    return true;
  }
};

struct foreign {
  using in = inputs<other_schema, other_x>;

  static constexpr bool holds(in)
  {
    return true;
  }
};

static_assert(some_rule<ordered, schema>);
static_assert(some_rule<positive, schema>);
static_assert(!some_rule<counting, schema>);
static_assert(!some_rule<blind, schema>);
static_assert(!some_rule<foreign, schema>);
static_assert(!some_rule<ordered, other_schema>);

// `all_hold` holds values to every rule of a list, and an empty list to none.
consteval bool test_all_hold()
{
  image<schema> values;
  if (!all_hold(typelist<ordered, positive>{}, values)) return false;
  if (!values.set<limit_low>(std::uint32_t{30})) return false;
  if (all_hold(typelist<ordered, positive>{}, values)) return false;
  return all_hold(typelist<positive>{}, values)
      && all_hold(typelist<>{}, values);
}

static_assert(test_all_hold());

} // namespace
