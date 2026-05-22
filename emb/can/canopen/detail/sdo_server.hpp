#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>

#include <emb/can.hpp>
#include <emb/can/bus.hpp>
#include <emb/container/inplace_queue.hpp>

#include "../od.hpp"
#include "../types.hpp"

namespace emb {
namespace can {
namespace canopen {
namespace detail {

class sdo_server {
public:
  static constexpr std::size_t tsdo_queue_capacity = 16;

  sdo_server(transport& bus, node_id node, std::span<od_entry> dictionary);

  sdo_server(sdo_server const&) = delete;
  sdo_server& operator=(sdo_server const&) = delete;

  bool try_handle(frame_t const& frame);

  void drain();

private:
  static constexpr od_key restore_default_parameter_key = {0x1011, 0x04};

  void init_dictionary();

  od_entry const* find(od_key key) const;

  std::expected<expedited_sdo, sdo_abort_code>
  read_expedited(od_entry const* entry, expedited_sdo const& rsdo);

  std::expected<expedited_sdo, sdo_abort_code>
  write_expedited(od_entry const* entry, expedited_sdo const& rsdo);

  std::expected<void, sdo_abort_code> restore_default_parameter(od_key key);

  transport& bus_;
  std::span<od_entry> dictionary_;
  id_t rsdo_cob_id_;
  id_t tsdo_cob_id_;
  emb::inplace_queue<payload_t, tsdo_queue_capacity> tsdo_queue_;
};

} // namespace detail
} // namespace canopen
} // namespace can
} // namespace emb
