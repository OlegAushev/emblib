#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <utility>

#include <emb/can.hpp>
#include <emb/can/bus.hpp>
#include <emb/container/inplace_queue.hpp>

#include "../od.hpp"
#include "../od_dictionary.hpp"
#include "../types.hpp"

namespace emb {
namespace can {
namespace canopen {
namespace detail {

template<typename Ctx>
class sdo_server {
public:
  static constexpr std::size_t tsdo_queue_capacity = 16;

  sdo_server(transport& bus, node_id node, od_view<Ctx> dictionary, Ctx& ctx)
      : bus_(bus),
        rsdo_cob_id_(cob_id_of<cob_type::rsdo>(node)),
        tsdo_cob_id_(cob_id_of<cob_type::tsdo>(node)),
        dictionary_(dictionary),
        ctx_(ctx)
  {
    bus_.add_filter(format_t::standard, rsdo_cob_id_, 0x7FF);
  }

  sdo_server(transport& bus,
             node_id node,
             od_view<Ctx> dictionary,
             Ctx&& ctx) = delete;

  sdo_server(sdo_server const&) = delete;
  sdo_server& operator=(sdo_server const&) = delete;

  bool try_handle(frame_t const& frame)
  {
    if (frame.id != rsdo_cob_id_) return false;

    expedited_sdo rsdo = from_payload<expedited_sdo>(frame.payload);
    if (rsdo.cs == sdo_cs_codes::abort) return true;

    od_key const key = {static_cast<std::uint16_t>(rsdo.index),
                        static_cast<std::uint8_t>(rsdo.subindex)};

    auto result = [&]() -> std::expected<expedited_sdo, sdo_abort_code> {
      auto const entry = dictionary_.find(key);
      if (!entry) return std::unexpected(entry.error());
      if (rsdo.cs == sdo_cs_codes::client_init_read)
        return read_expedited(*entry, rsdo);
      if (rsdo.cs == sdo_cs_codes::client_init_write)
        return write_expedited(*entry, rsdo);
      return std::unexpected(sdo_abort_code::invalid_cs);
    }();

    payload_t response =
        result ? to_payload<expedited_sdo>(*result)
               : to_payload<abort_sdo>(
                     abort_sdo{rsdo.index, rsdo.subindex, result.error()});

    if (!tsdo_queue_.full()) {
      tsdo_queue_.push(response);
    }
    return true;
  }

  void drain()
  {
    while (!tsdo_queue_.empty()) {
      frame_t frame = {.format = format_t::standard,
                       .id = tsdo_cob_id_,
                       .len = 8,
                       .payload = tsdo_queue_.front()};
      if (!bus_.send(frame)) return;
      tsdo_queue_.pop();
    }
  }

private:
  std::expected<expedited_sdo, sdo_abort_code>
  read_expedited(od_entry<Ctx> const& entry, expedited_sdo const& rsdo)
  {
    if (!od_readable(entry.access))
      return std::unexpected(sdo_abort_code::read_from_write_only);

    auto const value = entry.read(ctx_, entry.arg);
    if (!value) return std::unexpected(value.error());
    assert(value->index() == std::to_underlying(entry.type)
           && "od: a reader returned another type than its object's");

    expedited_sdo tsdo;
    tsdo.data = to_sdo_data(*value);

    std::size_t const data_size = od_size_of(entry.type);

    tsdo.index = rsdo.index;
    tsdo.subindex = rsdo.subindex;
    tsdo.cs = sdo_cs_codes::server_init_read;
    tsdo.expedited_transfer = 1;
    tsdo.data_size_indicated = 1;
    tsdo.data_empty_bytes = (4 - data_size) & 0x3;
    return tsdo;
  }

  std::expected<expedited_sdo, sdo_abort_code>
  write_expedited(od_entry<Ctx> const& entry, expedited_sdo const& rsdo)
  {
    if (!od_writable(entry.access))
      return std::unexpected(sdo_abort_code::write_to_read_only);
    if (!rsdo.expedited_transfer)
      return std::unexpected(sdo_abort_code::unsupported_access);
    if (rsdo.data_size_indicated
        && (4 - std::size_t{rsdo.data_empty_bytes}) != od_size_of(entry.type))
      return std::unexpected(sdo_abort_code::data_type_mismatch);

    od_value value = from_sdo_data(rsdo.data, entry.type);
    if (auto r = entry.write(ctx_, entry.arg, value); !r) {
      return std::unexpected(r.error());
    }

    expedited_sdo tsdo;
    tsdo.index = rsdo.index;
    tsdo.subindex = rsdo.subindex;
    tsdo.cs = sdo_cs_codes::server_init_write;
    return tsdo;
  }

  transport& bus_;
  id_t rsdo_cob_id_;
  id_t tsdo_cob_id_;
  od_view<Ctx> dictionary_;
  Ctx& ctx_;
  emb::inplace_queue<payload_t, tsdo_queue_capacity> tsdo_queue_;
};

} // namespace detail
} // namespace canopen
} // namespace can
} // namespace emb
