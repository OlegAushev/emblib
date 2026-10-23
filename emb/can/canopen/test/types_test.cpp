#include <emb/can/canopen/detail/nmt_slave.hpp>
#include <emb/can/canopen/types.hpp>

#include <cstdint>
#include <type_traits>

namespace {

using namespace emb::can::canopen;
using emb::can::format_t;
using emb::can::frame_t;

// -- node_id ------------------------------------------------------------------

static_assert(!node_id::make(0));
static_assert(node_id::make(1)->get() == 1);
static_assert(node_id::make(127)->get() == 127);
static_assert(!node_id::make(128));
static_assert(!node_id::make(256 + 5)); // checked before the narrowing cast
static_assert(node_id{5} == *node_id::make(5));
static_assert(node_id{5} != node_id{6});

static_assert(sizeof(node_id) == 1);
static_assert(std::is_trivially_copyable_v<node_id>);
static_assert(!std::is_default_constructible_v<node_id>);
static_assert(!std::is_convertible_v<int, node_id>); // explicit

// -- cob_id_of ----------------------------------------------------------------

inline constexpr node_id first{1};
inline constexpr node_id last{127};

static_assert(cob_id_of<cob_type::nmt>() == 0x000);
static_assert(cob_id_of<cob_type::sync>() == 0x080);
static_assert(cob_id_of<cob_type::emcy>(first) == 0x081);
static_assert(cob_id_of<cob_type::tsdo>(first) == 0x581);
static_assert(cob_id_of<cob_type::rsdo>(last) == 0x67F);
static_assert(cob_id_of<cob_type::heartbeat>(last) == 0x77F);
static_assert(cob_id_of<cob_type::tpdo, 1>(first) == 0x181);
static_assert(cob_id_of<cob_type::tpdo, 4>(last) == 0x4FF);
static_assert(cob_id_of<cob_type::rpdo, 1>(first) == 0x201);
static_assert(cob_id_of<cob_type::rpdo, 4>(last) == 0x57F);

template<cob_type C>
concept addressed_without_node = requires { cob_id_of<C>(); };

template<cob_type C>
concept addressed_by_node = requires(node_id n) { cob_id_of<C>(n); };

static_assert(addressed_without_node<cob_type::nmt>);
static_assert(!addressed_without_node<cob_type::heartbeat>);
static_assert(addressed_by_node<cob_type::heartbeat>);
static_assert(!addressed_by_node<cob_type::nmt>);
static_assert(!addressed_by_node<cob_type::tpdo>); // a PDO needs its number

// -- nmt_slave ----------------------------------------------------------------

constexpr frame_t nmt_frame(std::uint8_t cs, std::uint8_t target)
{
  return {.format = format_t::standard,
          .id = cob_id_of<cob_type::nmt>(),
          .len = 2,
          .payload = {cs, target}};
}

// Node 5 obeys a command for itself and one for all nodes, and ignores one
// for node 6.
consteval bool test_nmt_target()
{
  detail::nmt_slave const nmt{node_id{5}};
  return nmt.decode(nmt_frame(0x01, 5)) == nmt_command::start
      && nmt.decode(nmt_frame(0x02, 0)) == nmt_command::stop
      && !nmt.decode(nmt_frame(0x01, 6));
}

static_assert(test_nmt_target());

} // namespace
