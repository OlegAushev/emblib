#include "sdo_server.hpp"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <utility>

namespace emb {
namespace can {
namespace canopen {
namespace detail {

sdo_server::sdo_server(
    transport& bus,
    node_id node,
    std::span<od_entry> dictionary
)
    : bus_(bus),
      dictionary_(dictionary),
      rsdo_cob_id_(cob_id_of<cob_type::rsdo>(node)),
      tsdo_cob_id_(cob_id_of<cob_type::tsdo>(node)) {
  init_dictionary();
  bus_.add_filter(format_t::standard, rsdo_cob_id_, 0x7FF);
}

void sdo_server::init_dictionary() {
  std::sort(dictionary_.begin(), dictionary_.end());

  for (size_t i = 0; i < dictionary_.size(); ++i) {
    [[maybe_unused]] auto const& e = dictionary_[i];
    [[maybe_unused]] auto const& obj = e.object;

    if (i + 1 < dictionary_.size()) {
      [[maybe_unused]] auto const& next = dictionary_[i + 1];
      assert(!(e.key == next.key) && "od: duplicate {index, subindex}");
    }

    assert(
        (obj.read != nullptr || obj.write != nullptr)
        && "od: entry has no access method"
    );
  }
}

od_entry const* sdo_server::find(od_key key) const {
  auto it = std::lower_bound(dictionary_.begin(), dictionary_.end(), key);
  if (it == dictionary_.end() || !(key == *it)) return nullptr;
  return &(*it);
}

bool sdo_server::try_handle(frame_t const& frame) {
  if (frame.id != rsdo_cob_id_) return false;

  expedited_sdo rsdo = from_payload<expedited_sdo>(frame.payload);
  if (rsdo.cs == sdo_cs_codes::abort) return true;

  od_key key = {
      static_cast<uint16_t>(rsdo.index),
      static_cast<uint8_t>(rsdo.subindex)
  };
  od_entry const* entry = find(key);

  auto result = [&]() -> std::expected<expedited_sdo, sdo_abort_code> {
    if (!entry) return std::unexpected(sdo_abort_code::object_not_found);
    if (rsdo.cs == sdo_cs_codes::client_init_read)
      return read_expedited(entry, rsdo);
    if (rsdo.cs == sdo_cs_codes::client_init_write)
      return write_expedited(entry, rsdo);
    return std::unexpected(sdo_abort_code::invalid_cs);
  }();

  payload_t response = result ? to_payload<expedited_sdo>(*result)
                              : to_payload<abort_sdo>(abort_sdo{
                                    rsdo.index,
                                    rsdo.subindex,
                                    result.error()
                                });

  if (!tsdo_queue_.full()) {
    tsdo_queue_.push(response);
  }
  return true;
}

void sdo_server::drain() {
  while (!tsdo_queue_.empty()) {
    frame_t frame = {
        .format = format_t::standard,
        .id = tsdo_cob_id_,
        .len = 8,
        .payload = tsdo_queue_.front()
    };
    if (!bus_.send(frame)) return;
    tsdo_queue_.pop();
  }
}

std::expected<expedited_sdo, sdo_abort_code>
sdo_server::read_expedited(od_entry const* entry, expedited_sdo const& rsdo) {
  auto const& obj = entry->object;
  if (!obj.has_read_permission())
    return std::unexpected(sdo_abort_code::read_access_wo);

  auto value = obj.read();
  if (!value) return std::unexpected(value.error());

  expedited_sdo tsdo;
  tsdo.data = to_raw(*value);

  size_t const data_size =
      od_data_type_sizes[std::to_underlying(obj.data_type)];

  tsdo.index = rsdo.index;
  tsdo.subindex = rsdo.subindex;
  tsdo.cs = sdo_cs_codes::server_init_read;
  tsdo.expedited_transfer = 1;
  tsdo.data_size_indicated = 1;
  tsdo.data_empty_bytes = (4 - data_size) & 0x3;
  return tsdo;
}

std::expected<expedited_sdo, sdo_abort_code>
sdo_server::write_expedited(od_entry const* entry, expedited_sdo const& rsdo) {
  auto const& obj = entry->object;
  if (!obj.has_write_permission())
    return std::unexpected(sdo_abort_code::write_access_ro);

  // Special case: a write to 0x1011:4 is a restore-defaults request —
  // the SDO data carries the od_key of the parameter to restore, not a
  // value. Handled inline; the entry's write_func is not invoked.
  if (entry->key == restore_default_parameter_key) {
    od_key target = {};
    std::memcpy(&target, rsdo.data.data(), sizeof(target));
    if (auto r = restore_default_parameter(target); !r) {
      return std::unexpected(r.error());
    }
  } else {
    od_value value = make_od_value(rsdo.data, obj.data_type);
    if (auto r = obj.write(value); !r) {
      return std::unexpected(r.error());
    }
  }

  expedited_sdo tsdo;
  tsdo.index = rsdo.index;
  tsdo.subindex = rsdo.subindex;
  tsdo.cs = sdo_cs_codes::server_init_write;
  return tsdo;
}

std::expected<void, sdo_abort_code>
sdo_server::restore_default_parameter(od_key key) {
  od_entry const* entry = find(key);
  if (entry == nullptr) {
    return std::unexpected(sdo_abort_code::object_not_found);
  }

  auto const& obj = entry->object;

  if (!obj.default_value.has_value()) {
    return std::unexpected(sdo_abort_code::data_store_error);
  }

  if (!obj.has_write_permission()) {
    return std::unexpected(sdo_abort_code::write_access_ro);
  }

  return obj.write(*obj.default_value);
}

} // namespace detail
} // namespace canopen
} // namespace can
} // namespace emb
