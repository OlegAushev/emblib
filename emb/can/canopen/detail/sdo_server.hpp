#pragma once

#include <algorithm>
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

template<std::uint8_t NodeId, typename Ctx>
class sdo_server {
public:
  static constexpr std::size_t tsdo_queue_capacity = 16;

  sdo_server(transport& bus, od_view<Ctx> dictionary, Ctx& ctx)
      : bus_(bus), dictionary_(dictionary), ctx_(ctx)
  {
    bus_.add_filter(format_t::standard, rsdo_cob_id_, 0x7FF);
  }

  sdo_server(transport& bus, od_view<Ctx> dictionary, Ctx&& ctx) = delete;

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
      // A write to 1011h:04 is a restore-default request: the SDO data
      // carries the key of the object to restore, not a value. Handled here
      // entirely; the dictionary need not have an entry for it.
      if (rsdo.cs == sdo_cs_codes::client_init_write
          && key == od_restore_default_key) {
        return write_restore_default(rsdo);
      }
      od_entry<Ctx> const* entry = dictionary_.find(key);
      if (!entry) return std::unexpected(sdo_abort_code::object_not_found);
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
  static constexpr id_t rsdo_cob_id_ = cob_id_of<cob_type::rsdo, NodeId>();
  static constexpr id_t tsdo_cob_id_ = cob_id_of<cob_type::tsdo, NodeId>();

  // Where the last read of a string object stopped. A string travels 4 bytes
  // per read; reading another string object starts over from its first word,
  // and so does the read after the word that holds the terminating NUL.
  struct text_cursor {
    od_key key{};
    std::uint16_t word = 0;
  };

  std::uint16_t text_word(od_key key)
  {
    if (text_.key != key) {
      text_ = {key, 0};
    }
    return text_.word;
  }

  void advance_text(expedited_sdo_data const& data)
  {
    bool const last =
        std::ranges::any_of(data, [](std::uint8_t byte) { return byte == 0; });
    text_.word =
        last ? std::uint16_t{0} : static_cast<std::uint16_t>(text_.word + 1);
  }

  std::expected<expedited_sdo, sdo_abort_code>
  read_expedited(od_entry<Ctx> const& entry, expedited_sdo const& rsdo)
  {
    if (!od_readable(entry.access))
      return std::unexpected(sdo_abort_code::read_from_write_only);

    bool const text = entry.type == od_value_type::string;
    auto const value =
        entry.read(ctx_, text ? text_word(entry.key()) : entry.arg);
    if (!value) return std::unexpected(value.error());
    assert(value->index() == od_alternative_of(entry.type)
           && "od: a reader returned another type than its object's");

    expedited_sdo tsdo;
    tsdo.data = to_raw(*value);
    if (text) {
      advance_text(tsdo.data);
    }

    std::size_t const data_size =
        od_data_type_sizes[std::to_underlying(entry.type)];

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

    od_value value = make_od_value(rsdo.data, entry.type);
    if (auto r = entry.write(ctx_, entry.arg, value); !r) {
      return std::unexpected(r.error());
    }

    expedited_sdo tsdo;
    tsdo.index = rsdo.index;
    tsdo.subindex = rsdo.subindex;
    tsdo.cs = sdo_cs_codes::server_init_write;
    return tsdo;
  }

  std::expected<expedited_sdo, sdo_abort_code>
  write_restore_default(expedited_sdo const& rsdo)
  {
    od_key const target = {
        static_cast<std::uint16_t>(rsdo.data[0] | (rsdo.data[1] << 8)),
        rsdo.data[2]};
    if (auto r = restore_default(target); !r) {
      return std::unexpected(r.error());
    }

    expedited_sdo tsdo;
    tsdo.index = rsdo.index;
    tsdo.subindex = rsdo.subindex;
    tsdo.cs = sdo_cs_codes::server_init_write;
    return tsdo;
  }

  od_write_result restore_default(od_key key)
  {
    od_entry<Ctx> const* entry = dictionary_.find(key);
    if (entry == nullptr) {
      return std::unexpected(sdo_abort_code::object_not_found);
    }
    if (!od_writable(entry->access)) {
      return std::unexpected(sdo_abort_code::write_to_read_only);
    }
    if (!entry->restorable) {
      return std::unexpected(sdo_abort_code::no_data_available);
    }
    return dictionary_.restore()(ctx_, entry->arg);
  }

  transport& bus_;
  od_view<Ctx> dictionary_;
  Ctx& ctx_;
  text_cursor text_;
  emb::inplace_queue<payload_t, tsdo_queue_capacity> tsdo_queue_;
};

} // namespace detail
} // namespace canopen
} // namespace can
} // namespace emb
